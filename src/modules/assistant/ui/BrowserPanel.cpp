#include "BrowserPanel.h"
#include "modules/assistant/runtime/tools/ToolAbort.h"
#include <QColor>
#include <QPalette>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineNewWindowRequest>
#include <QWebEnginePermission>
#include <QWebEngineWebAuthUxRequest>
#include <QAuthenticator>
#include <QInputDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <QThread>
#include <QApplication>
#include <QPointer>
#include <QMutex>
#include <QWaitCondition>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <atomic>

namespace {
QString permissionLabel(QWebEnginePermission::PermissionType type)
{
    using P = QWebEnginePermission::PermissionType;
    switch (type) {
    case P::MediaAudioCapture: return QStringLiteral("麦克风");
    case P::MediaVideoCapture: return QStringLiteral("摄像头");
    case P::MediaAudioVideoCapture: return QStringLiteral("摄像头和麦克风");
    case P::DesktopVideoCapture: return QStringLiteral("屏幕共享");
    case P::DesktopAudioVideoCapture: return QStringLiteral("屏幕与音频共享");
    case P::MouseLock: return QStringLiteral("锁定鼠标");
    case P::Notifications: return QStringLiteral("发送通知");
    case P::Geolocation: return QStringLiteral("位置信息");
    case P::ClipboardReadWrite: return QStringLiteral("读写剪贴板");
    case P::LocalFontsAccess: return QStringLiteral("本地字体");
    default: return QStringLiteral("未知权限");
    }
}

bool browserIsDarkTheme()
{
    // 应用通过 ThemeManager 设置全局调色板，这里直接读窗口底色判断明暗，
    // 避免 BrowserPanel 依赖 core 主题模块，便于单测独立链接。
    return QApplication::palette().color(QPalette::Window).lightness() < 128;
}

constexpr int kNavIconPx = 18;
constexpr int kNavButtonPx = 28;
// 地址栏保留足够高度，避免全局输入框样式裁切文字。
constexpr int kAddressHeightPx = 32;

QString browserForegroundColor()
{
    return browserIsDarkTheme() ? QStringLiteral("#e5e7eb") : QStringLiteral("#374151");
}

QString browserNavButtonStyle()
{
    const bool dark = browserIsDarkTheme();
    const QString text = browserForegroundColor();
    const QString bg = dark ? "rgba(255,255,255,0.06)" : "rgba(15,23,42,0.05)";
    const QString hover = dark ? "rgba(255,255,255,0.12)" : "rgba(15,23,42,0.10)";
    return QStringLiteral(
        "QPushButton{border:none;border-radius:6px;background:%1;color:%2;padding:0;}"
        "QPushButton:hover{background:%3;}")
        .arg(bg, text, hover);
}

// 图标是单色透明底的 PNG，用 SourceIn 直接换成当前主题的前景色。
// BrowserPanel 不依赖 core 主题模块（便于单测独立链接），所以这里就地取色。
QIcon browserNavIcon(const QString &fileName)
{
    QPixmap pixmap(QStringLiteral(":/images/") + fileName);
    if (pixmap.isNull()) {
        return QIcon();
    }
    // 源图四周留了透明边，先裁掉再缩放，图标才能填满按钮。
    const QImage image = pixmap.toImage();
    int left = image.width(), top = image.height(), right = -1, bottom = -1;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(image.pixel(x, y)) > 8) {
                left = qMin(left, x);
                right = qMax(right, x);
                top = qMin(top, y);
                bottom = qMax(bottom, y);
            }
        }
    }
    if (right >= left && bottom >= top) {
        pixmap = pixmap.copy(left, top, right - left + 1, bottom - top + 1);
    }
    pixmap = pixmap.scaled(kNavIconPx, kNavIconPx, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QPainter painter(&pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), QColor(browserForegroundColor()));
    painter.end();
    return QIcon(pixmap);
}

// 起始页：介绍 browser use 功能的静态页面，随资源一起打包。
QUrl browserHomeUrl()
{
    return QUrl(QStringLiteral("qrc:/html/browser-home.html"));
}

// 交给用户后若一直没完成认证，避免 Agent 工作线程无限期阻塞。
constexpr qint64 kManualWaitTimeoutMs = 5 * 60 * 1000;
// 交出控制权后的最短等待时间：用户动手需要时间，不能刚交出去就抢回来。
constexpr qint64 kManualMinWaitMs = 5000;

class BrowserPage final : public QWebEnginePage {
public:
    using QWebEnginePage::QWebEnginePage;
protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType, bool) override {
        if (url == browserHomeUrl()) return true;  // 自带起始页，其余 qrc 资源一律不放行
        return url.scheme() == "https" || url.scheme() == "http" || url == QUrl("about:blank");
    }
};

// 认证页特征检测：密码框、验证码、扫码登录、人机验证等需要用户介入的信号。
// 自动接管恢复与主脚本共用同一份判断，避免两处条件漂移。
const char authDetection[] = R"JS(
 const ntVisible = e => !!(e.getClientRects().length && getComputedStyle(e).visibility !== 'hidden');
 const ntSensitive = e => e.matches('input[type=password],input[autocomplete=one-time-code]') ||
   /password|passwd|otp|verification|验证码|校验码|动态码/i.test([e.name,e.id,e.autocomplete,e.placeholder,e.getAttribute('aria-label')].join(' '));
 const ntNeedsUser = () => {
   const inputs = Array.from(document.querySelectorAll('input')).filter(ntVisible);
   const pageText = document.body ? document.body.innerText : '';
   return inputs.some(ntSensitive) || /扫码登录|扫描二维码.*登录|人机验证|安全验证|verify you are human|scan.{0,30}(sign in|log in)|enter.{0,20}verification code/i.test(pageText) ||
     Array.from(document.querySelectorAll('iframe')).some(e => ntVisible(e) && /captcha|challenge/i.test(e.src));
 };
)JS";

// 轮询用：页面是否仍停在需要用户接管的认证环节。
const char authProbe[] = R"JS(
(() => {
%1
 return ntNeedsUser();
})()
)JS";

// The isolated world owns element references; websites cannot forge tool handles.
// Check authentication before returning text or executing any DOM operation.
const char script[] = R"JS(
(() => {
 const args = %2;
%1
 if (ntNeedsUser())
   return JSON.stringify({needsUser:true});
 let state = globalThis.__ntBrowserState;
 if (['click','fill','select'].includes(args.action)) {
   if (!state || args.snapshot !== state.id || state.url !== location.href || state.dirty)
     return JSON.stringify({error:'页面已变化，请重新 read 获取元素编号'});
   const e = state.elements[args.element - 1];
   if (!e || !e.isConnected || !ntVisible(e) || e.disabled)
     return JSON.stringify({error:'元素不可操作，请重新 read'});
   if (ntSensitive(e) || e.matches('input[type=file]')) return JSON.stringify({needsUser:true});
   if (args.action === 'click') { e.click(); }
   else if (args.action === 'select') {
     if (!(e instanceof HTMLSelectElement) || !Array.from(e.options).some(o => o.value === args.text))
       return JSON.stringify({error:'请选择有效的 select 选项值'});
     e.value = args.text;
     e.dispatchEvent(new Event('input',{bubbles:true})); e.dispatchEvent(new Event('change',{bubbles:true}));
   } else {
     if (!(e instanceof HTMLInputElement || e instanceof HTMLTextAreaElement) || e.readOnly ||
         (e instanceof HTMLInputElement && !['text','search','email','url','tel','number'].includes(e.type)))
       return JSON.stringify({error:'该元素不是可填写的普通输入框'});
     const proto = e instanceof HTMLInputElement ? HTMLInputElement.prototype : HTMLTextAreaElement.prototype;
     Object.getOwnPropertyDescriptor(proto,'value').set.call(e, args.text || '');
     e.dispatchEvent(new Event('input',{bubbles:true})); e.dispatchEvent(new Event('change',{bubbles:true}));
   }
   if (state.observer) state.observer.disconnect();
   globalThis.__ntBrowserState = null;
   return JSON.stringify({acted:true});
 }
 if (args.action === 'scroll') {
   window.scrollBy(0,(args.text === 'up' ? -1 : 1)*innerHeight*0.8);
   return JSON.stringify({acted:true});
 }
 const elements = Array.from(document.querySelectorAll('a[href],button,input,textarea,select,[role=button]')).filter(ntVisible).slice(0,160);
 const id = Date.now().toString(36) + Math.random().toString(36).slice(2);
 if (state && state.observer) state.observer.disconnect();
 state = {id,elements,url:location.href,dirty:false};
 state.observer = new MutationObserver(() => {state.dirty = true;});
 state.observer.observe(document.documentElement,{subtree:true,childList:true,attributes:true,characterData:true});
 globalThis.__ntBrowserState = state;
 const walker = document.createTreeWalker(document.body || document.documentElement, NodeFilter.SHOW_TEXT);
 const text = []; let length = 0;
 while (walker.nextNode() && length < 18000) {
   const node = walker.currentNode, parent = node.parentElement;
   if (!parent || parent.closest('script,style,noscript,input,textarea,[contenteditable]') || !ntVisible(parent)) continue;
   const value = node.textContent.trim(); if (value) { text.push(value); length += value.length + 1; }
 }
 return JSON.stringify({snapshot:id,url:location.href,title:document.title,content:text.join('\n').slice(0,18000),
   elements:elements.map((e,i)=>({element:i+1,tag:e.tagName.toLowerCase(),type:e.type || '',
     label:(e.innerText || e.getAttribute('aria-label') || e.placeholder || e.name || '').slice(0,180),
     options:e instanceof HTMLSelectElement ? Array.from(e.options).slice(0,40).map(o=>({text:o.text,value:o.value})) : undefined}))});
})()
)JS";
}

struct BrowserPanel::Request {
    QMutex mutex;
    QWaitCondition ready;
    QString result;
    QJsonObject args;
    bool done = false;
    std::atomic_bool cancelled{false};
    QElapsedTimer elapsed;
};

BrowserPanel::BrowserPanel(QWidget *parent) : QWidget(parent)
{
    setMinimumWidth(320);
    // 除 header 外整个区域都留给网页：不留外边距，也不放状态栏。
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto *header = new QWidget(this);
    auto *bar = new QHBoxLayout(header);
    bar->setContentsMargins(8, 6, 8, 6);
    bar->setSpacing(4);
    const QString navStyle = browserNavButtonStyle();
    auto navButton = [&](const QString &iconFile, const QString &tip) {
        auto *b = new QPushButton(this);
        b->setStyleSheet(navStyle);
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(tip);
        b->setIcon(browserNavIcon(iconFile));
        b->setIconSize(QSize(kNavIconPx, kNavIconPx));
        b->setFixedSize(kNavButtonPx, kNavButtonPx);
        return b;
    };
    auto *collapse = navButton(QStringLiteral("collapse.png"), QStringLiteral("收起浏览器，恢复对话窗口宽度"));
    auto *back = navButton(QStringLiteral("back.png"), QStringLiteral("后退"));
    auto *forward = navButton(QStringLiteral("forward.png"), QStringLiteral("前进"));
    auto *reload = navButton(QStringLiteral("refresh.png"), QStringLiteral("刷新页面"));
    auto *home = navButton(QStringLiteral("home.png"), QStringLiteral("回到主页"));
    address_ = new QLineEdit(this);
    address_->setPlaceholderText(QStringLiteral("输入网址"));
    address_->setFixedHeight(kAddressHeightPx);
    // 全局 QLineEdit 样式带 4px 上下内边距 + 1px 边框，配上 28px 的固定高度会把
    // 文字裁掉一半；这里收紧内边距，让 32px 里的可用高度足够放下 14px 的字。
    address_->setStyleSheet(QStringLiteral("QLineEdit{padding:2px 10px;}"));
    bar->addWidget(collapse);
    bar->addWidget(back);
    bar->addWidget(forward);
    bar->addWidget(reload);
    bar->addWidget(home);
    bar->addWidget(address_, 1);
    auto *external = navButton(QStringLiteral("open_external.png"), QStringLiteral("在系统浏览器中打开"));
    bar->addWidget(external);
    connect(collapse, &QPushButton::clicked, this, &BrowserPanel::collapseRequested);
    layout->addWidget(header);
    view_ = new QWebEngineView(this);
    profile_ = new QWebEngineProfile(this);
    view_->setPage(new BrowserPage(profile_, view_));
    view_->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, false);
    view_->settings()->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, false);
    installHomeThemeScript();
    layout->addWidget(view_, 1);
    loadHome();  // 起始页 = 功能说明，Home 按钮也回到这里
    // 没有“接管 / 已完成，继续 / 停止操作”按钮：只有遇到认证才自动交给用户，
    // 页面离开认证环节后自动继续；停止由对话输入框的中断按钮统一触发。
    // 用户自己点这些按钮属于正常浏览，不打断 AI。
    connect(address_,&QLineEdit::returnPressed,this,[this]{ navigate(address_->text()); });
    connect(back,&QPushButton::clicked,this,[this]{ view_->back(); });
    connect(forward,&QPushButton::clicked,this,[this]{ view_->forward(); });
    connect(reload,&QPushButton::clicked,this,[this]{ view_->reload(); });
    connect(home,&QPushButton::clicked,this,[this]{ loadHome(); });
    connect(external,&QPushButton::clicked,this,[this]{
        const auto url = view_->url();
        if (url.scheme() == "http" || url.scheme() == "https") QDesktopServices::openUrl(url);
    });
    connect(view_,&QWebEngineView::urlChanged,this,[this](const QUrl &url){
        // 起始页是内部资源，地址栏留空更像一个正常的主页。
        address_->setText(url == browserHomeUrl() ? QString() : url.toDisplayString(QUrl::RemoveUserInfo));
        ++generation_;
    });
    connect(view_,&QWebEngineView::loadStarted,this,[this]{ loading_ = true; ++generation_; });
    connect(view_,&QWebEngineView::loadFinished,this,[this](bool ok){
        loading_ = false;
        settled_.start(); settleMs_ = 1200;
        if (!ok && request_ && !manual_) finish(QStringLiteral("网页加载失败，可点击刷新重试。"),true);
    });
    connect(view_->page(),&QWebEnginePage::renderProcessTerminated,this,[this]{ loading_ = false; finish(QStringLiteral("浏览器渲染进程退出，请刷新重试。"),true); });
    connect(view_->page(),&QWebEnginePage::newWindowRequested,this,[this](QWebEngineNewWindowRequest &r){
        // target=_blank 很常见，直接在本视图里打开，不必因此打断 AI。
        const auto url = r.requestedUrl();
        if (url.scheme() == "http" || url.scheme() == "https") view_->load(url);
    });
    connect(view_->page(),&QWebEnginePage::authenticationRequired,this,[this](const QUrl &, QAuthenticator *auth){
        takeOver(QStringLiteral("网站需要 HTTP 身份认证，请输入账号和密码。"));
        bool ok = false;
        const QString user = QInputDialog::getText(this,QStringLiteral("网站认证"),QStringLiteral("账号"),QLineEdit::Normal,{},&ok);
        if (!ok) return;
        const QString password = QInputDialog::getText(this,QStringLiteral("网站认证"),QStringLiteral("密码"),QLineEdit::Password,{},&ok);
        if (ok) { auth->setUser(user); auth->setPassword(password); }
    });
    connect(view_->page(),&QWebEnginePage::permissionRequested,this,[this](QWebEnginePermission permission){
        takeOver(QStringLiteral("网站请求设备权限，请确认后继续。"));
        const auto answer = QMessageBox::question(this,QStringLiteral("网站权限"),
            QStringLiteral("允许 %1 使用%2？").arg(permission.origin().toDisplayString(),permissionLabel(permission.permissionType())),
            QMessageBox::Yes|QMessageBox::No,QMessageBox::No);
        if (answer == QMessageBox::Yes) permission.grant(); else permission.deny();
    });
    connect(view_->page(),&QWebEnginePage::webAuthUxRequested,this,[this](QWebEngineWebAuthUxRequest *request){
        takeOver(QStringLiteral("网站需要安全密钥或通行密钥认证，请完成认证后继续。"));
        QPointer<QWebEngineWebAuthUxRequest> guard(request);
        const auto handle = [this,guard]{
            if (!guard) return;
            using State = QWebEngineWebAuthUxRequest::WebAuthUxState;
            bool ok = false;
            if (guard->state() == State::SelectAccount) {
                const auto account = QInputDialog::getItem(this,QStringLiteral("通行密钥认证"),QStringLiteral("选择账号"),guard->userNames(),0,false,&ok);
                if (guard) { if (ok) guard->setSelectedAccount(account); else guard->cancel(); }
            } else if (guard->state() == State::CollectPin) {
                const auto pin = QInputDialog::getText(this,QStringLiteral("安全密钥认证"),QStringLiteral("输入安全密钥 PIN"),QLineEdit::Password,{},&ok);
                if (guard) { if (ok) guard->setPin(pin); else guard->cancel(); }
            } else if (guard->state() == State::RequestFailed) {
                // 没有底部状态栏了，认证失败这类结果直接发到对话里让用户看到。
                emit attentionRequired(QStringLiteral("安全密钥认证失败，请在网站重试或使用其他登录方式。"));
                guard->cancel();
            }
        };
        connect(request,&QWebEngineWebAuthUxRequest::stateChanged,this,[handle]{ handle(); },Qt::QueuedConnection);
        QTimer::singleShot(0,this,handle);
    });
    timer_ = new QTimer(this);
    timer_->setInterval(200);
    connect(timer_,&QTimer::timeout,this,[this]{
        if (!request_) return;
        if (request_->cancelled) { finish(QStringLiteral("浏览器操作已取消")); view_->stop(); return; }
        if (manual_) {
            request_->elapsed.restart();  // 等待用户期间不计入 30 秒步骤超时
            checkAutoResume();
            return;
        }
        if (request_->elapsed.elapsed() > 30000) { finish(QStringLiteral("浏览器操作超时，可点击刷新重试。"),true); view_->stop(); return; }
        if (!loading_ && !evaluating_ && (!settled_.isValid() || settled_.elapsed() >= settleMs_)) observe();
    });
    timer_->start();
}

BrowserPanel::~BrowserPanel()
{
    ++generation_;
    finish(QStringLiteral("浏览器已关闭"));
    delete view_; // page must die before its profile
    delete profile_;
}

QString BrowserPanel::execute(const QJsonObject &args, LlmTools::ToolAbort *abort)
{
    if (QThread::currentThread() == thread()) return QStringLiteral("浏览工具须由 Agent 工作线程调用");
    auto request = std::make_shared<Request>();
    QMetaObject::invokeMethod(this,[this,args,request]{ if (!request->cancelled) start(args,request); },Qt::QueuedConnection);
    QMutexLocker lock(&request->mutex);
    while (!request->done) {
        if (abort && abort->isAborted()) { request->cancelled = true; return QStringLiteral("浏览器操作已取消"); }
        request->ready.wait(&request->mutex,50);
    }
    return request->result;
}

void BrowserPanel::start(const QJsonObject &args, const std::shared_ptr<Request> &request)
{
    finish(QStringLiteral("浏览器操作已被替换"));
    request_ = request;
    request_->args = args;
    request_->elapsed.start();
    emit activityRequested();
    const QString action = args.value("action").toString();
    if (!QStringList{"open","read","click","fill","select","scroll","back","wait_user"}.contains(action)) {
        finish(QStringLiteral("不支持的浏览器操作")); return;
    }
    if (manual_ || action == "wait_user") {
        takeOver(QStringLiteral("请在右侧完成登录或验证码，完成后 AI 会自动继续。")); return;
    }
    if (action == "open") {
        const QUrl url(args.value("url").toString());
        if (!url.isValid() || url.host().isEmpty() || !url.userInfo().isEmpty() || (url.scheme() != "https" && url.scheme() != "http")) {
            finish(QStringLiteral("仅支持不含账号密码的有效 HTTP/HTTPS 地址")); return;
        }
        request_->args = QJsonObject{{"action","read"}};
        view_->load(url);
    } else if (action == "back") {
        request_->args = QJsonObject{{"action","read"}};
        view_->back();
    }
}

void BrowserPanel::navigate(const QString &text)
{
    const auto url = QUrl::fromUserInput(text);
    if ((url.scheme() == "http" || url.scheme() == "https") && url.userInfo().isEmpty()) view_->load(url);
}

void BrowserPanel::loadHome()
{
    // 走一次真正的导航，而不是 setHtml：Home 按钮因此和普通浏览一样有历史记录、
    // 可刷新，也不会出现“已经在主页上再点一次看不出变化”之外的各种歧义。
    view_->load(browserHomeUrl());
}

// 起始页跟随应用主题。WebEngine 的 prefers-color-scheme 读的是系统设置，
// 和应用当前主题不一定一致，所以这里由应用把当前主题写进 <html>。
void BrowserPanel::installHomeThemeScript()
{
    // 必须在 DocumentReady 注入：文档创建时 <html> 还没被解析出来，那时设的属性
    // 会被解析器新建的元素整个丢掉（实测仍停在 HTML 里写死的 light）。
    // 带上 URL 判断，免得给用户访问的每个网站都塞一个 data- 属性。
    const QString source = QStringLiteral(
        "if(location.href.indexOf('qrc:/html/browser-home.html')===0){"
        "document.documentElement.setAttribute('data-nt-theme','%1');}")
        .arg(browserIsDarkTheme() ? QStringLiteral("dark") : QStringLiteral("light"));

    QWebEngineScript script;
    script.setName(QStringLiteral("ntHomeTheme"));
    script.setInjectionPoint(QWebEngineScript::DocumentReady);
    // 主世界：属性要参与页面的 CSS 匹配。
    script.setWorldId(QWebEngineScript::MainWorld);
    script.setRunsOnSubFrames(false);
    script.setSourceCode(source);

    const auto existing = profile_->scripts()->find(QStringLiteral("ntHomeTheme"));
    for (const auto &old : existing) {
        profile_->scripts()->remove(old);
    }
    profile_->scripts()->insert(script);
}

void BrowserPanel::observe()
{
    if (!request_ || manual_) return;
    evaluating_ = true;
    const auto request = request_;
    const auto generation = generation_;
    const auto source = QString::fromUtf8(script)
        .arg(QString::fromUtf8(authDetection),
             QString::fromUtf8(QJsonDocument(request->args).toJson(QJsonDocument::Compact)));
    QPointer<BrowserPanel> guard(this);
    view_->page()->runJavaScript(source,QWebEngineScript::ApplicationWorld,[this,guard,request,generation](const QVariant &value){
        if (!guard) return;
        evaluating_ = false;
        if (request_ != request || manual_ || request->cancelled) return;
        if (generation != generation_) { request_->args = QJsonObject{{"action","read"}}; return; }
        const QString result = value.toString();
        const auto object = QJsonDocument::fromJson(result.toUtf8()).object();
        if (object.value("needsUser").toBool()) {
            takeOver(QStringLiteral("检测到密码、验证码或认证页面，请你在右侧完成；完成后 AI 会自动继续。"));
        } else if (object.value("acted").toBool()) {
            request_->args = QJsonObject{{"action","read"}};
            settled_.start(); settleMs_ = 350;
        } else if (!object.isEmpty()) {
            finish(result);
        }
    });
}

void BrowserPanel::takeOver(const QString &reason)
{
    const bool notify = !manual_;
    if (!manual_) manualWait_.start();
    manual_ = true;
    ++generation_;
    if (notify) emit attentionRequired(reason);
}

void BrowserPanel::resume()
{
    manual_ = false;
    ++generation_;
    if (request_) { request_->args = QJsonObject{{"action","read"}}; request_->elapsed.restart(); }
}

// 页面离开认证环节后自动把控制权交还 AI，无需任何按钮。
void BrowserPanel::checkAutoResume()
{
    if (!manual_ || !request_ || evaluating_ || loading_) return;
    if (manualWait_.isValid() && manualWait_.elapsed() > kManualWaitTimeoutMs) {
        finish(QStringLiteral("等待用户完成认证超时，请重试或稍后再让我操作。"),true);
        return;
    }
    // 刚交出控制权时先让用户动手，别立刻抢回来。
    if (!manualWait_.isValid() || manualWait_.elapsed() < kManualMinWaitMs) return;
    if (settled_.isValid() && settled_.elapsed() < settleMs_) return;
    evaluating_ = true;
    const auto request = request_;
    const auto generation = generation_;
    QPointer<BrowserPanel> guard(this);
    const auto source = QString::fromUtf8(authProbe).arg(QString::fromUtf8(authDetection));
    view_->page()->runJavaScript(source,QWebEngineScript::ApplicationWorld,[this,guard,request,generation](const QVariant &value){
        if (!guard) return;
        evaluating_ = false;
        if (!manual_ || request_ != request || request->cancelled) return;
        if (generation != generation_) return;
        if (value.toBool()) return;  // 仍在认证页，继续等用户
        resume();
    });
}

void BrowserPanel::cancel()
{
    finish(QStringLiteral("浏览器操作已取消，不要自动重试，等待用户指示。"));
    view_->stop();
    // 取消后不再挂起，否则用户的下一条指令会被卡在“等待接管”上。
    manual_ = false;
}

// notifyUser 用于失败、超时等终态：面板底部已经没有状态栏，这些结果改成发一条
// 对话消息，否则用户只会看到操作无声无息地停掉。
void BrowserPanel::finish(const QString &result, bool notifyUser)
{
    if (!request_) return;
    auto request = std::move(request_);
    QMutexLocker lock(&request->mutex);
    request->result = result;
    request->done = true;
    request->ready.wakeAll();
    if (notifyUser) emit attentionRequired(result);
}

