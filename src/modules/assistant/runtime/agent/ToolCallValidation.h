#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QUrl>
#include <cmath>

namespace Agent::ToolCallValidation {

inline QJsonObject error(const QString &code, const QString &tool,
                         const QString &field, const QString &message)
{
    return {{"code", code}, {"tool", tool}, {"field", field}, {"message", message},
            {"retryable", true}, {"executed", false},
            {"instruction", QStringLiteral("Correct the call using the supplied tool schema and native tool_calls. No tool in this batch was executed. Do not emit DSML/XML or Markdown tool calls.")}};
}

// Validate the schema keywords used by our tool definitions; never coerce arguments.
inline QJsonObject validateValue(const QJsonValue &value, const QJsonObject &schema,
                                 const QString &tool, const QString &path)
{
    const auto fail = [&](const QString &message) {
        return error(QStringLiteral("INVALID_TOOL_ARGUMENTS"), tool, path, message);
    };
    const QString type = schema.value("type").toString();
    const bool matches = type.isEmpty()
        || (type == "object" && value.isObject()) || (type == "array" && value.isArray())
        || (type == "string" && value.isString()) || (type == "boolean" && value.isBool())
        || (type == "null" && value.isNull()) || (type == "number" && value.isDouble())
        || (type == "integer" && value.isDouble() && std::floor(value.toDouble()) == value.toDouble());
    if (!matches) return fail(QStringLiteral("Expected %1").arg(type));
    if (schema.contains("enum") && !schema.value("enum").toArray().contains(value))
        return fail(QStringLiteral("Value is not in the allowed enum"));
    if (value.isDouble()) {
        if (schema.contains("minimum") && value.toDouble() < schema.value("minimum").toDouble())
            return fail(QStringLiteral("Value is below minimum"));
        if (schema.contains("maximum") && value.toDouble() > schema.value("maximum").toDouble())
            return fail(QStringLiteral("Value is above maximum"));
    }
    if (value.isObject()) {
        const auto object = value.toObject();
        const auto properties = schema.value("properties").toObject();
        for (const auto &required : schema.value("required").toArray()) {
            if (!object.contains(required.toString()))
                return error(QStringLiteral("INVALID_TOOL_ARGUMENTS"), tool,
                             path + "." + required.toString(), QStringLiteral("Required property is missing"));
        }
        for (auto it = object.begin(); it != object.end(); ++it) {
            if (!properties.contains(it.key())) {
                if (schema.value("additionalProperties") == QJsonValue(false))
                    return fail(QStringLiteral("Unknown property: %1").arg(it.key()));
                continue;
            }
            const auto issue = validateValue(it.value(), properties.value(it.key()).toObject(), tool, path + "." + it.key());
            if (!issue.isEmpty()) return issue;
            if (it.key() == "url" && it.value().isString() && !it.value().toString().isEmpty()) {
                const QString text = it.value().toString();
                const QUrl url(text, QUrl::StrictMode);
                if (text != text.trimmed() || text.startsWith('[') || !url.isValid() || url.scheme().isEmpty())
                    return error(QStringLiteral("INVALID_TOOL_ARGUMENTS"), tool, path + ".url",
                                 QStringLiteral("Expected a plain absolute URL, not a Markdown link"));
            }
        }
    } else if (value.isArray()) {
        const auto array = value.toArray();
        for (int i = 0; i < array.size(); ++i) {
            const auto issue = validateValue(array.at(i), schema.value("items").toObject(), tool,
                                             path + QStringLiteral("[%1]").arg(i));
            if (!issue.isEmpty()) return issue;
        }
    }
    return {};
}

inline QJsonObject arguments(const QString &tool, const QString &json, const QJsonObject &schema)
{
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return error(QStringLiteral("INVALID_TOOL_ARGUMENTS"), tool, QStringLiteral("arguments"),
                     QStringLiteral("Expected a JSON object: %1").arg(parseError.errorString()));
    return validateValue(document.object(), schema, tool, QStringLiteral("arguments"));
}

inline QJsonObject batch(const QJsonArray &calls, const QJsonArray &definitions)
{
    QSet<QString> ids;
    for (const auto &value : calls) {
        const auto call = value.toObject();
        const auto function = call.value("function").toObject();
        const QString name = function.value("name").toString();
        const QString id = call.value("id").toString();
        if (id.isEmpty() || ids.contains(id) || call.value("type") != QStringLiteral("function")
            || !function.value("arguments").isString())
            return error(QStringLiteral("INVALID_TOOL_CALL"), name, QStringLiteral("tool_calls"),
                         QStringLiteral("Expected unique nonempty id, type=function and string arguments"));
        ids.insert(id);
        bool found = false;
        for (const auto &definition : definitions) {
            const auto spec = definition.toObject().value("function").toObject();
            if (spec.value("name") != name) continue;
            found = true;
            const auto issue = arguments(name, function.value("arguments").toString(), spec.value("parameters").toObject());
            if (!issue.isEmpty()) return issue;
            break;
        }
        if (!found) return error(QStringLiteral("UNKNOWN_TOOL"), name, QStringLiteral("function.name"),
                                 QStringLiteral("unregistered tool; choose a supplied tool name"));
    }
    return {};
}

// Hold a possible protocol marker across SSE chunk boundaries, including escaped closers.
inline QStringList markers() {
    return {QStringLiteral("<｜｜DSML｜｜"), QStringLiteral("<|DSML|"),
            QStringLiteral("<tool_call"), QStringLiteral("<function_call")};
}
inline int safeTextLength(const QString &text) {
    int length = text.size();
    for (const auto &marker : markers()) {
        const auto index = text.indexOf(marker);
        if (index >= 0) length = qMin(length, int(index));
        for (int n = 1; n < marker.size(); ++n)
            if (text.endsWith(marker.left(n))) length = qMin(length, int(text.size()) - n);
    }
    return length;
}
inline bool hasProtocolText(const QString &text) {
    for (const auto &marker : markers()) if (text.contains(marker)) return true;
    return false;
}

} // namespace Agent::ToolCallValidation
