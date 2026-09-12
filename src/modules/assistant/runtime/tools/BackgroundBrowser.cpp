#include "BackgroundBrowser.h"
#include "ToolAbort.h"

#include <QElapsedTimer>
#include <QJsonDocument>
#include <QLocale>
#include <QMutex>
#include <QThread>
#include <QTimer>
#include <QWaitCondition>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlRequestInfo>
#include <atomic>
#include <memory>

namespace LlmTools {
namespace {

QString acceptLanguageHeader()
{
    const QLocale sys = QLocale::system();
    QString primary = sys.bcp47Name();
    if (primary.isEmpty()) {
        primary = QStringLiteral("en-US");
    }
    const QString lang = primary.section(QLatin1Char('-'), 0, 0);
    QString header = primary;
    if (!lang.isEmpty() && lang.compare(primary, Qt::CaseInsensitive) != 0) {
        header += QLatin1Char(',') + lang + QStringLiteral(";q=0.9");
    }
    if (lang.compare(QStringLiteral("en"), Qt::CaseInsensitive) != 0) {
        header += QStringLiteral(",en;q=0.8");
    }
    return header;
}

QString contentExtractionScript(int maxChars)
{
    return QStringLiteral(R"JS(
        JSON.stringify({url: location.href, title: document.title,
            content: (document.body ? document.body.innerText : '').slice(0, %1),
            links: Array.from(document.querySelectorAll('a[href]'))
                .filter(a => /^https?:/.test(a.href) && a.innerText.trim())
                .slice(0, 40).map(a => ({text:a.innerText.slice(0,200),url:a.href}))})
    )JS").arg(maxChars);
}

QString articleExtractionScript()
{
    return QStringLiteral(R"JS(
        (function() {
            var selectors = ['article', 'main', '[role=main]', '.article', '.article-content',
                '.post-body', '.entry-content', '.post', '.content', '.markdown-body', '#content', '#main'];
            var best = null, bestLen = -1;
            selectors.forEach(function(sel) {
                var els = document.querySelectorAll(sel);
                for (var i = 0; i < els.length; i++) {
                    var len = (els[i].innerText || '').length;
                    if (len > bestLen) { bestLen = len; best = els[i]; }
                }
            });
            var root = best || document.body;
            var clone = root.cloneNode(true);
            clone.querySelectorAll('script,style,noscript,iframe,svg,canvas,button,form,nav,aside,footer,header,figure,img,video,audio')
                .forEach(function(e) { e.remove(); });
            var text = (clone.innerText || clone.textContent || '')
                .replace(/[ \t]+/g, ' ').replace(/\n{3,}/g, '\n\n').trim();
            return JSON.stringify({url: location.href, title: document.title, content: text.slice(0, 30000)});
        })()
    )JS");
}

QString wrapScript(const QString &script)
{
    return QStringLiteral(
        "(function() {\n"
        "try {\n"
        "  var __r = (function() {\n")
        + script
        + QStringLiteral(
        "\n  })();\n"
        "  if (__r === undefined) return 'undefined';\n"
        "  if (typeof __r === 'function') return String(__r);\n"
        "  try { var __s = JSON.stringify(__r); return (__s === undefined) ? String(__r) : __s; }\n"
        "  catch (e) { return String(__r); }\n"
        "} catch (e) {\n"
        "  return 'JS_ERROR: ' + (e && e.message ? e.message : String(e));\n"
        "}\n"
        "})()");
}

struct Result {
    QMutex mutex;
    QWaitCondition ready;
    std::atomic_bool cancelled{false};
    bool done = false;
    QString text;
};

class ReadPage final : public QWebEnginePage {
public:
    using QWebEnginePage::QWebEnginePage;
protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType, bool) override
    {
        return url.scheme() == QStringLiteral("https") || url.scheme() == QStringLiteral("http");
    }
};

// 文本抽取不需要图片/字体/媒体等重资源，拦截它们可显著加速页面就绪。
class BlockerInterceptor final : public QWebEngineUrlRequestInterceptor {
public:
    explicit BlockerInterceptor(QObject *parent = nullptr)
        : QWebEngineUrlRequestInterceptor(parent) {}
    void interceptRequest(QWebEngineUrlRequestInfo &info) override
    {
        switch (info.resourceType()) {
        case QWebEngineUrlRequestInfo::ResourceTypeImage:
        case QWebEngineUrlRequestInfo::ResourceTypeFontResource:
        case QWebEngineUrlRequestInfo::ResourceTypeMedia:
        case QWebEngineUrlRequestInfo::ResourceTypeFavicon:
        case QWebEngineUrlRequestInfo::ResourceTypePing:
        case QWebEngineUrlRequestInfo::ResourceTypeCspReport:
            info.block(true);
            break;
        default:
            break;
        }
    }
};

class Request final : public QObject {
public:
    Request(QObject *owner, std::shared_ptr<Result> result, const QUrl &url,
            const QString &script, int maxResultChars)
        : QObject(owner), result_(std::move(result)), script_(script), maxResultChars_(maxResultChars)
    {
        // Unnamed profiles are off the record. Destroy the page before its profile.
        profile_ = new QWebEngineProfile(this);
        profile_->setHttpAcceptLanguage(acceptLanguageHeader());
        profile_->setUrlRequestInterceptor(new BlockerInterceptor(this));
        page_ = new ReadPage(profile_, this);
        page_->setAudioMuted(true);
        page_->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, false);
        page_->settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false);

        connect(page_, &QWebEnginePage::renderProcessTerminated, this, [this] {
            finish(QStringLiteral("网页渲染进程退出"));
        });
        // loadFinished 不再等图片/字体/媒体等重资源（已被拦截器拦截），
        // 因此这里 `load` 事件基本等价于「DOM 解析 + 脚本执行完成」，比原来快很多。
        // 抽取前仍留一段 settle：无头 QWebEnginePage 属于「隐藏页」，Chromium 会把
        // setTimeout 等定时器节流到约 1000ms，过短会抽到客户端 JS 渲染前的旧内容。
        connect(page_, &QWebEnginePage::loadFinished, this, [this](bool ok) {
            if (!ok) { finish(QStringLiteral("网页加载失败")); return; }
            scheduleExtract(1200);
        });

        timer_.setInterval(50);
        connect(&timer_, &QTimer::timeout, this, [this] {
            if (result_->cancelled || elapsed_.elapsed() >= 25000)
                finish(QStringLiteral("网页访问已取消或超时"));
        });

        elapsed_.start();
        timer_.start();
        page_->load(url);
    }
    ~Request() override
    {
        finished_ = true;
        delete page_;
        delete profile_;
        QMutexLocker lock(&result_->mutex);
        if (!result_->done) {
            result_->text = QStringLiteral("网页访问已关闭");
            result_->done = true;
            result_->ready.wakeAll();
        }
    }
private:
    void scheduleExtract(int delayMs)
    {
        if (finished_ || extracting_) return;
        extracting_ = true;
        QTimer::singleShot(delayMs, this, [this] {
            if (finished_) return;
            page_->runJavaScript(script_, [this](const QVariant &value) {
                if (finished_) return;
                QString text = value.toString();
                if (text.size() > maxResultChars_) {
                    text.truncate(maxResultChars_);
                }
                finish(text.isEmpty() ? QStringLiteral("无法读取网页正文") : text);
            });
        });
    }
    void finish(const QString &text)
    {
        if (finished_) return;
        finished_ = true;
        timer_.stop();
        page_->triggerAction(QWebEnginePage::Stop);
        {
            QMutexLocker lock(&result_->mutex);
            result_->text = text;
            result_->done = true;
            result_->ready.wakeAll();
        }
        deleteLater();
    }
    std::shared_ptr<Result> result_;
    QWebEngineProfile *profile_ = nullptr;
    QWebEnginePage *page_ = nullptr;
    QTimer timer_;
    QElapsedTimer elapsed_;
    QString script_;
    int maxResultChars_ = 64000;
    bool finished_ = false;
    bool extracting_ = false;
};
}

QString BackgroundBrowser::read(const QUrl &url, int maxChars, ToolAbort *abort)
{
    return run(url, contentExtractionScript(qBound(1024, maxChars, 64000)), 64000, abort);
}

QString BackgroundBrowser::readArticle(const QUrl &url, ToolAbort *abort)
{
    return run(url, articleExtractionScript(), 64000, abort);
}

QString BackgroundBrowser::evaluate(const QUrl &url, const QString &script, ToolAbort *abort)
{
    if (script.trimmed().isEmpty()) {
        return QStringLiteral("脚本不能为空");
    }
    return run(url, wrapScript(script), 64000, abort);
}

QString BackgroundBrowser::run(const QUrl &url, const QString &script, int maxResultChars, ToolAbort *abort)
{
    if (!url.isValid() || url.host().isEmpty() ||
        (url.scheme() != QStringLiteral("http") && url.scheme() != QStringLiteral("https")))
        return QStringLiteral("仅支持有效的 HTTP/HTTPS 网页地址");
    if (QThread::currentThread() == thread())
        return QStringLiteral("浏览工具必须从 Agent 工作线程调用");
    auto result = std::make_shared<Result>();
    QMetaObject::invokeMethod(this, [this, result, url, script, maxResultChars] {
        if (!result->cancelled) new Request(this, result, url, script, maxResultChars);
    }, Qt::QueuedConnection);
    QElapsedTimer elapsed;
    elapsed.start();
    QMutexLocker lock(&result->mutex);
    while (!result->done) {
        if ((abort && abort->isAborted()) || elapsed.elapsed() >= 27000) {
            result->cancelled = true;
            return QStringLiteral("网页访问已取消或超时");
        }
        result->ready.wait(&result->mutex, 50);
    }
    return result->text;
}
}
