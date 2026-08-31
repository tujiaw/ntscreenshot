#include "WebSearchTool.h"

#include "core/settings/SettingModel.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QTimer>
#include <QUrl>

namespace {

constexpr int kDefaultTimeoutMs = 20 * 1000;
constexpr int kDefaultMaxResults = 5;
constexpr auto kTavilyProviderId = "tavily";
constexpr auto kTavilyProviderName = "Tavily";
constexpr auto kTavilySearchUrl = "https://api.tavily.com/search";

class TavilyWebSearchProvider {
public:
    QString search(const QString &query, const QString &apiKey, int maxResults) const;
};

QString TavilyWebSearchProvider::search(const QString &query, const QString &apiKey, int maxResults) const
{
    QJsonObject requestBody;
    requestBody.insert(QStringLiteral("query"), query);
    requestBody.insert(QStringLiteral("max_results"), qMax(1, maxResults));
    requestBody.insert(QStringLiteral("search_depth"), QStringLiteral("advanced"));

    QNetworkAccessManager manager;
    QNetworkRequest request{QUrl(QString::fromLatin1(kTavilySearchUrl))};
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(apiKey).toUtf8());
    request.setTransferTimeout(kDefaultTimeoutMs);

    QSslConfiguration sslConfiguration(QSslConfiguration::defaultConfiguration());
    sslConfiguration.setProtocol(QSsl::TlsV1_2);
    request.setSslConfiguration(sslConfiguration);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QNetworkReply *reply = manager.post(request, QJsonDocument(requestBody).toJson(QJsonDocument::Compact));
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
            "# Web Search Result\n\n"
            "- Provider: %1\n"
            "- URL: %2\n"
            "- Error: 请求超时")
            .arg(QString::fromLatin1(kTavilyProviderName), QString::fromLatin1(kTavilySearchUrl));
    }

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray responseData = reply->readAll();
    const QString errorString = reply->error() == QNetworkReply::NoError ? QString() : reply->errorString();
    reply->deleteLater();

    if (!errorString.isEmpty()) {
        return QStringLiteral(
            "# Web Search Result\n\n"
            "- Provider: %1\n"
            "- URL: %2\n"
            "- HTTP Status: %3\n"
            "- Error: %4")
            .arg(QString::fromLatin1(kTavilyProviderName),
                 QString::fromLatin1(kTavilySearchUrl),
                 statusCode > 0 ? QString::number(statusCode) : QStringLiteral("N/A"),
                 errorString);
    }

    return QString::fromUtf8(responseData).trimmed();
}

} // namespace

namespace LlmTools {

WebSearchTool::WebSearchTool(SettingModel* settings)
    : settings_(settings)
{
}

QString WebSearchTool::name() const
{
    return QStringLiteral("web_search");
}

QString WebSearchTool::description() const
{
    return QStringLiteral("网络搜索，适合需要最新网页结果、搜索引擎摘要或外部资料时使用。");
}

QJsonObject WebSearchTool::parameters() const
{
    QJsonObject properties;
    properties.insert(QStringLiteral("query"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("string")},
        {QStringLiteral("description"), QStringLiteral("要搜索的关键词或问题")}
    });
    properties.insert(QStringLiteral("max_results"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("integer")},
        {QStringLiteral("description"), QStringLiteral("可选，期望返回的结果条数，默认 5")}
    });

    QJsonObject parametersObject;
    parametersObject.insert(QStringLiteral("type"), QStringLiteral("object"));
    parametersObject.insert(QStringLiteral("properties"), properties);
    parametersObject.insert(QStringLiteral("required"), QJsonArray{QStringLiteral("query")});
    parametersObject.insert(QStringLiteral("additionalProperties"), false);
    return parametersObject;
}

QString WebSearchTool::execute(const QJsonObject &arguments) const
{
    const QString query = arguments.value(QStringLiteral("query")).toString().trimmed();
    const int maxResults = qMax(1, arguments.value(QStringLiteral("max_results")).toInt(kDefaultMaxResults));

    if (query.isEmpty()) {
        return argumentError(QStringLiteral("缺少 query 参数"));
    }

    SettingModel *setting = settings_;
    if (!setting) {
        return QStringLiteral("# Tool Error\n\n- Tool: `web_search`\n- Error: 设置模块不可用");
    }

    const QString provider = setting->webSearchProvider();
    const QString apiKey = setting->webSearchApiKey();
    if (apiKey.isEmpty()) {
        return QStringLiteral("# Tool Error\n\n- Tool: `web_search`\n- Error: 网络搜索 API KEY 为空");
    }

    if (provider.compare(QString::fromLatin1(kTavilyProviderId), Qt::CaseInsensitive) == 0) {
        return TavilyWebSearchProvider().search(query, apiKey, maxResults);
    }

    return QStringLiteral("# Tool Error\n\n- Tool: `web_search`\n- Error: 不支持的网络搜索服务商：%1")
        .arg(provider);
}

} // namespace LlmTools
