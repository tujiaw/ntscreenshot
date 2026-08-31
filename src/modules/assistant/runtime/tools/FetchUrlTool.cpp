#include "FetchUrlTool.h"

#include "LlmToolUtils.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>

namespace {

constexpr int kDefaultFetchMaxChars = 20 * 1024;
constexpr int kMaxFetchChars = 128 * 1024;
constexpr int kDefaultTimeoutMs = 15 * 1000;
constexpr auto kFetchUserAgent =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/136.0.0.0 Safari/537.36 ntscreenshot/1.0";

QString convertHtmlToMarkdown(const QString &html)
{
    QString sanitized = html;
    sanitized.remove(QRegularExpression(QStringLiteral("<script\\b[^>]*>[\\s\\S]*?</script>"),
                                        QRegularExpression::CaseInsensitiveOption));
    sanitized.remove(QRegularExpression(QStringLiteral("<style\\b[^>]*>[\\s\\S]*?</style>"),
                                        QRegularExpression::CaseInsensitiveOption));
    sanitized.remove(QRegularExpression(QStringLiteral("<noscript\\b[^>]*>[\\s\\S]*?</noscript>"),
                                        QRegularExpression::CaseInsensitiveOption));

    QTextDocument document;
    document.setHtml(sanitized);
    QString markdown = document.toMarkdown(QTextDocument::MarkdownDialectCommonMark);
    if (markdown.trimmed().isEmpty()) {
        markdown = document.toPlainText();
    }
    return LlmTools::collapseBlankLines(markdown);
}

} // namespace

namespace LlmTools {

QString FetchUrlTool::name() const
{
    return QStringLiteral("fetch_url");
}

QString FetchUrlTool::description() const
{
    return QStringLiteral("获取 URL 内容，并尽量转换为适合模型阅读的 Markdown 文本。适合网页、JSON 和纯文本内容。");
}

QJsonObject FetchUrlTool::parameters() const
{
    QJsonObject parametersObject;
    parametersObject.insert(QStringLiteral("type"), QStringLiteral("object"));
    parametersObject.insert(QStringLiteral("properties"), QJsonObject{
        {QStringLiteral("url"), QJsonObject{
            {QStringLiteral("type"), QStringLiteral("string")},
            {QStringLiteral("description"), QStringLiteral("要抓取的网页或文本链接")}
        }},
        {QStringLiteral("max_chars"), QJsonObject{
            {QStringLiteral("type"), QStringLiteral("integer")},
            {QStringLiteral("description"), QStringLiteral("返回内容的最大字符数，默认 20000")}
        }}
    });
    parametersObject.insert(QStringLiteral("required"), QJsonArray{QStringLiteral("url")});
    parametersObject.insert(QStringLiteral("additionalProperties"), false);
    return parametersObject;
}

QString FetchUrlTool::execute(const QJsonObject &arguments) const
{
    const QString urlText = arguments.value(QStringLiteral("url")).toString().trimmed();
    const int maxChars = clampInt(arguments.value(QStringLiteral("max_chars")).toInt(kDefaultFetchMaxChars),
                                  1024,
                                  kMaxFetchChars);

    if (urlText.isEmpty()) {
        return argumentError(QStringLiteral("缺少 url 参数"));
    }

    const QUrl url = QUrl::fromUserInput(urlText);
    if (!url.isValid() || url.scheme().isEmpty()) {
        return argumentError(QStringLiteral("url 参数无效"));
    }

    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(kFetchUserAgent));
    request.setRawHeader("Accept",
                         "text/html,application/xhtml+xml,application/xml;q=0.9,"
                         "application/json,text/plain;q=0.8,*/*;q=0.7");
    request.setRawHeader("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
    request.setRawHeader("Cache-Control", "no-cache");
    request.setRawHeader("Pragma", "no-cache");
    request.setRawHeader("Upgrade-Insecure-Requests", "1");
    request.setTransferTimeout(kDefaultTimeoutMs);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QNetworkReply *reply = manager.get(request);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(kDefaultTimeoutMs);
    loop.exec();
    
    if (timer.isActive()) {
        timer.stop();
    } else {
        reply->abort();
        reply->deleteLater();
        return QStringLiteral(
            "# Fetch URL Result\n\n"
            "- URL: %1\n"
            "- Error: 请求超时")
            .arg(url.toString());
    }

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    const QByteArray data = reply->readAll();
    const QString errorString = reply->error() == QNetworkReply::NoError ? QString() : reply->errorString();
    reply->deleteLater();

    if (!errorString.isEmpty()) {
        return QStringLiteral(
            "# Fetch URL Result\n\n"
            "- URL: %1\n"
            "- HTTP Status: %2\n"
            "- Error: %3")
            .arg(url.toString())
            .arg(statusCode > 0 ? QString::number(statusCode) : QStringLiteral("N/A"))
            .arg(errorString);
    }

    QString body;
    QString displayType = contentType.isEmpty() ? QStringLiteral("unknown") : contentType;
    if (contentType.contains(QStringLiteral("html"), Qt::CaseInsensitive)) {
        body = convertHtmlToMarkdown(QString::fromUtf8(data));
    } else if (contentType.contains(QStringLiteral("json"), Qt::CaseInsensitive)) {
        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        body = parseError.error == QJsonParseError::NoError
            ? QString::fromUtf8(doc.toJson(QJsonDocument::Indented))
            : QString::fromUtf8(data);
        body = QStringLiteral("```json\n%1\n```").arg(body.trimmed());
    } else if (contentType.startsWith(QStringLiteral("text/"), Qt::CaseInsensitive) ||
               contentType.contains(QStringLiteral("xml"), Qt::CaseInsensitive) ||
               contentType.contains(QStringLiteral("javascript"), Qt::CaseInsensitive)) {
        body = QStringLiteral("```text\n%1\n```").arg(QString::fromUtf8(data).trimmed());
    } else if (looksBinary(data)) {
        body = QStringLiteral("[二进制内容，无法直接转换为 Markdown 文本]");
    } else {
        body = QStringLiteral("```text\n%1\n```").arg(QString::fromUtf8(data).trimmed());
    }

    bool truncated = false;
    body = truncateText(body, maxChars, &truncated);

    return QStringLiteral(
        "# Fetch URL Result\n\n"
        "- URL: %1\n"
        "- HTTP Status: %2\n"
        "- Content-Type: %3\n"
        "- Truncated: %4\n\n"
        "## Content\n\n%5")
        .arg(url.toString())
        .arg(statusCode > 0 ? QString::number(statusCode) : QStringLiteral("N/A"))
        .arg(displayType)
        .arg(truncated ? QStringLiteral("yes") : QStringLiteral("no"))
        .arg(body);
}

} // namespace LlmTools
