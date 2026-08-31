#include "ConversationHistory.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QStringList>

namespace {

const char *kRole = "role";
const char *kContent = "content";
const char *kType = "type";
const char *kText = "text";
const char *kToolCalls = "tool_calls";
const char *kImageUrl = "image_url";

QString extractText(const QJsonValue &contentVal)
{
    if (contentVal.isString()) {
        return contentVal.toString();
    }
    if (!contentVal.isArray()) {
        return {};
    }

    QStringList parts;
    for (const QJsonValue &item : contentVal.toArray()) {
        const QJsonObject obj = item.toObject();
        const QString type = obj.value(QLatin1String(kType)).toString();
        if (type == QLatin1String(kText) || type.isEmpty()) {
            const QString text = obj.value(QLatin1String(kText)).toString();
            if (!text.isEmpty()) {
                parts.append(text);
            }
        } else if (type == QLatin1String(kImageUrl)) {
            parts.append(QStringLiteral("[图片]"));
        }
    }
    return parts.join(QLatin1Char(' '));
}

QJsonValue stripImageContent(const QJsonValue &contentVal)
{
    if (!contentVal.isArray()) {
        return contentVal;
    }

    QJsonArray stripped;
    bool sawImage = false;
    for (const QJsonValue &item : contentVal.toArray()) {
        const QJsonObject obj = item.toObject();
        if (obj.value(QLatin1String(kType)).toString() == QLatin1String(kImageUrl)) {
            sawImage = true;
            continue;
        }
        stripped.append(item);
    }
    if (sawImage) {
        QJsonObject placeholder;
        placeholder.insert(QLatin1String(kType), QLatin1String(kText));
        placeholder.insert(QLatin1String(kText), QStringLiteral("[图片]"));
        stripped.prepend(placeholder);
    }
    if (stripped.size() == 1 && stripped.at(0).toObject().value(QLatin1String(kType)).toString() == QLatin1String(kText)) {
        return stripped.at(0).toObject().value(QLatin1String(kText));
    }
    return stripped;
}

int estimateTextTokens(const QString &text)
{
    if (text.isEmpty()) {
        return 0;
    }
    return qMax(1, (text.toUtf8().size() + 3) / 4);
}

QList<QJsonArray> splitRounds(const QJsonArray &messages)
{
    QList<QJsonArray> rounds;
    QJsonArray current;
    for (const QJsonValue &value : messages) {
        const QJsonObject msg = value.toObject();
        if (msg.value(QLatin1String(kRole)).toString() == QStringLiteral("user") && !current.isEmpty()) {
            rounds.append(current);
            current = QJsonArray();
        }
        current.append(msg);
    }
    if (!current.isEmpty()) {
        rounds.append(current);
    }
    return rounds;
}

QJsonArray concatRounds(const QList<QJsonArray> &rounds)
{
    QJsonArray out;
    for (const QJsonArray &round : rounds) {
        for (const QJsonValue &value : round) {
            out.append(value);
        }
    }
    return out;
}

} // namespace

namespace ConversationHistory {

int estimateTokens(const QJsonArray &messages)
{
    int total = 0;
    for (const QJsonValue &value : messages) {
        const QJsonObject msg = value.toObject();
        total += 4;
        total += estimateTextTokens(extractText(msg.value(QLatin1String(kContent))));
        const QJsonArray toolCalls = msg.value(QLatin1String(kToolCalls)).toArray();
        if (!toolCalls.isEmpty()) {
            total += estimateTextTokens(QString::fromUtf8(QJsonDocument(toolCalls).toJson(QJsonDocument::Compact)));
        }
    }
    return total;
}

QString messageToSummaryChunk(const QJsonObject &message)
{
    const QString role = message.value(QLatin1String(kRole)).toString();
    QString body = extractText(message.value(QLatin1String(kContent)));
    if (body.isEmpty()) {
        if (!message.value(QLatin1String(kToolCalls)).toArray().isEmpty()) {
            body = QStringLiteral("[assistant tool_calls]");
        } else {
            body = QStringLiteral("(无正文)");
        }
    }
    const QString label = role.isEmpty() ? QStringLiteral("unknown") : role;
    return QStringLiteral("[%1]\n%2\n").arg(label, body);
}

QJsonArray stripImagePayloads(const QJsonArray &messages)
{
    QJsonArray stripped;
    for (const QJsonValue &value : messages) {
        QJsonObject msg = value.toObject();
        if (msg.contains(QLatin1String(kContent))) {
            msg.insert(QLatin1String(kContent), stripImageContent(msg.value(QLatin1String(kContent))));
        }
        stripped.append(msg);
    }
    return stripped;
}

TrimResult trimToTokenBudget(const QJsonArray &messages, int maxPromptTokens, int lastPromptTokens)
{
    TrimResult result;
    result.kept = messages;
    if (messages.isEmpty() || maxPromptTokens <= 0) {
        return result;
    }

    const int estimated = estimateTokens(messages);
    const bool overBudget = (lastPromptTokens > maxPromptTokens) || (estimated > maxPromptTokens);
    if (!overBudget) {
        return result;
    }

    QList<QJsonArray> rounds = splitRounds(messages);
    if (rounds.size() <= 1) {
        return result;
    }

    if (lastPromptTokens > maxPromptTokens) {
        const QJsonArray dropped = rounds.takeFirst();
        for (const QJsonValue &value : dropped) {
            result.droppedChunks.append(messageToSummaryChunk(value.toObject()));
        }
    }

    while (rounds.size() > 1 && estimateTokens(concatRounds(rounds)) > maxPromptTokens) {
        const QJsonArray dropped = rounds.takeFirst();
        for (const QJsonValue &value : dropped) {
            result.droppedChunks.append(messageToSummaryChunk(value.toObject()));
        }
    }

    result.kept = concatRounds(rounds);
    return result;
}

} // namespace ConversationHistory
