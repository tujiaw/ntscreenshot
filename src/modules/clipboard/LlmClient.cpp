#include "LlmClient.h"

#include "core/network/HttpRequest.h"
#include <QDebug>

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLatin1String>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

LlmClient::LlmClient(const LlmConfig& config)
    : config_(config) {}

QByteArray LlmClient::BuildRequestBody(const QString& clipboardText,
                                       const QString& controlsJson) const {
    QJsonObject body;
    body[QLatin1String("model")] = config_.model;

    QJsonObject systemMsg;
    systemMsg[QLatin1String("role")] = "system";
    systemMsg[QLatin1String("content")] =
        "You are an intelligent form-filling assistant.\n"
        "Map information from the source text to the best matching editable form controls.\n\n"
        "You MUST call fill_fields. Do not output text, explanations, or markdown.\n\n"
        "Rules:\n"
        "- Use semantic meaning and all field context: name, label, automation_id, placeholder, current_value, nearby fields.\n"
        "- Parse structured data and natural language.\n"
        "- Extract, split, combine, and normalize values when appropriate, including names, addresses, dates, phones, emails, IDs, and amounts.\n"
        "- Prefer a reasonable source-supported inference over leaving a matching field empty.\n"
        "- If multiple mappings are plausible, choose the most likely one.\n"
        "- Never invent values not supported by the source text.\n"
        "- Do not overwrite current_value if it already appears correct.\n"
        "- Return only fields you intend to update.";

    QJsonObject userMsg;
    userMsg[QLatin1String("role")] = "user";
    QString userContent = QStringLiteral("## Source text\n```text\n%1\n```\n\n"
                                         "## Editable form controls\n```json\n%2\n```")
                             .arg(clipboardText, controlsJson);
    userMsg[QLatin1String("content")] = userContent;

    body[QLatin1String("messages")] = QJsonArray({systemMsg, userMsg});

    QJsonObject thinking;
    thinking[QLatin1String("type")] = "disabled";
    body[QLatin1String("thinking")] = thinking;

    QJsonObject controlIndexProps;
    controlIndexProps[QLatin1String("type")] = "integer";
    QJsonObject valueProps;
    valueProps[QLatin1String("type")] = "string";
    QJsonObject itemProps;
    itemProps[QLatin1String("control_index")] = controlIndexProps;
    itemProps[QLatin1String("value")] = valueProps;
    QJsonObject itemsObj;
    itemsObj[QLatin1String("type")] = "object";
    itemsObj[QLatin1String("properties")] = itemProps;
    itemsObj[QLatin1String("required")] = QJsonArray({QLatin1String("control_index"), QLatin1String("value")});
    QJsonObject fieldsObj;
    fieldsObj[QLatin1String("type")] = "array";
    fieldsObj[QLatin1String("items")] = itemsObj;
    QJsonObject propertiesObj;
    propertiesObj[QLatin1String("fields")] = fieldsObj;

    QJsonObject params;
    params[QLatin1String("type")] = "object";
    params[QLatin1String("properties")] = propertiesObj;
    params[QLatin1String("required")] = QJsonArray({QLatin1String("fields")});

    QJsonObject functionObj;
    functionObj[QLatin1String("name")] = "fill_fields";
    functionObj[QLatin1String("description")] = "Fill form fields with values from clipboard text";
    functionObj[QLatin1String("parameters")] = params;

    QJsonObject fillFunc;
    fillFunc[QLatin1String("type")] = "function";
    fillFunc[QLatin1String("function")] = functionObj;

    body[QLatin1String("tools")] = QJsonArray({fillFunc});
    body[QLatin1String("tool_choice")] = "auto";
    body[QLatin1String("thinking")] = QJsonObject({{"type", "disabled"}});

    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

QByteArray LlmClient::HttpPost(const QString& fullUrl, const QByteArray& body) {
    QNetworkAccessManager nam;
    QNetworkRequest request{QUrl(fullUrl)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", ("Bearer " + config_.apiKey).toUtf8());

    QNetworkReply* reply = nam.post(request, body);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const QByteArray response = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();

    qInfo().noquote() << QStringLiteral("[WinHttp] HTTP status: %1, body size: %2 bytes")
                             .arg(status).arg(response.size());

    if (status == 401 || status == 403) {
        lastError_ = QStringLiteral("API authentication failed. Please check your API key.");
        return {};
    }
    if (status != 200) {
        lastError_ = QStringLiteral("API returned error status: %1").arg(status);
        if (!response.isEmpty()) {
            lastError_ += QStringLiteral("\n") + QString::fromUtf8(response);
        }
        return {};
    }
    return response;
}

std::vector<FillResult> LlmClient::ParseResponse(const QByteArray& responseJson) {
    std::vector<FillResult> result;
    if (responseJson.isEmpty()) {
        return result;
    }

    QJsonParseError parseError{};
    const QJsonDocument resp = QJsonDocument::fromJson(responseJson, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        lastError_ = QStringLiteral("Failed to parse API response JSON: ") + parseError.errorString();
        return result;
    }
    if (!resp.isObject()) {
        lastError_ = QStringLiteral("Unexpected API response.");
        return result;
    }
    const QJsonObject root = resp.object();
    if (root.contains(QLatin1String("error"))) {
        lastError_ = root.value(QLatin1String("error")).toObject().value(QLatin1String("message")).toString(
            QStringLiteral("Unknown API error"));
        return result;
    }

    const QJsonArray choices = root.value(QLatin1String("choices")).toArray();
    if (choices.isEmpty()) {
        lastError_ = QStringLiteral("No choices in API response.");
        return result;
    }
    const QJsonObject message = choices.first().toObject().value(QLatin1String("message")).toObject();
    if (!message.contains(QLatin1String("tool_calls")) || message.value(QLatin1String("tool_calls")).toArray().isEmpty()) {
        return result;
    }
    const QJsonObject toolCall = message.value(QLatin1String("tool_calls")).toArray().first().toObject();
    if (toolCall.value(QLatin1String("type")).toString() != QStringLiteral("function") ||
        toolCall.value(QLatin1String("function")).toObject().value(QLatin1String("name")).toString() != QStringLiteral("fill_fields")) {
        return result;
    }

    const QString argsStr = toolCall.value(QLatin1String("function")).toObject().value(QLatin1String("arguments")).toString();
    const QJsonDocument argsDoc = QJsonDocument::fromJson(argsStr.toUtf8());
    if (!argsDoc.isObject()) {
        return result;
    }
    const QJsonArray fields = argsDoc.object().value(QLatin1String("fields")).toArray();
    for (const QJsonValue& field : fields) {
        FillResult fr;
        fr.controlIndex = field.toObject().value(QLatin1String("control_index")).toInt(-1);
        fr.value = field.toObject().value(QLatin1String("value")).toString();
        if (fr.controlIndex >= 0 && !fr.value.isEmpty()) {
            result.push_back(std::move(fr));
        }
    }
    return result;
}

std::vector<FillResult> LlmClient::RequestFill(const QString& clipboardText,
                                               const QString& controlsJson) {
    lastError_.clear();
    if (clipboardText.isEmpty()) {
        lastError_ = QStringLiteral("Clipboard text is empty.");
        return {};
    }

    const QByteArray body = BuildRequestBody(clipboardText, controlsJson);
    if (body.isEmpty()) {
        lastError_ = QStringLiteral("Failed to build request body.");
        return {};
    }

    const QString fullUrl = NetworkUrl::chatCompletions(config_.endpoint);

    qInfo().noquote() << QStringLiteral("========== LLM REQUEST ==========\nURL: %1\n%2")
                             .arg(fullUrl, QString::fromUtf8(body));

    const QByteArray response = HttpPost(fullUrl, body);
    if (!lastError_.isEmpty()) {
        return {};
    }

    qInfo().noquote() << QStringLiteral("========== LLM RESPONSE ==========\n%1")
                             .arg(QString::fromUtf8(response));

    auto results = ParseResponse(response);
    qInfo().noquote() << QStringLiteral("========== FILL RESULTS ==========\nTotal: %1")
                             .arg(results.size());
    return results;
}

