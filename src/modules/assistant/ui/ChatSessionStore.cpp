#include "ChatSessionStore.h"

#include "core/platform/Util.h"
#include "modules/assistant/runtime/agent/ConversationHistory.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QVariant>

namespace {

constexpr auto kSessionFileName = "assistant_chat_session.json";

} // namespace

namespace ChatSessionStore {

QString filePath()
{
    return QDir(Util::getWritebaleDir()).filePath(QString::fromLatin1(kSessionFileName));
}

bool load(Session *out)
{
    if (!out) {
        return false;
    }
    *out = Session{};
    QFile file(filePath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }
    const QJsonObject root = doc.object();
    out->summaryText = root.value(QStringLiteral("summaryText")).toString();
    out->conversationMessages = ConversationHistory::stripImagePayloads(
        root.value(QStringLiteral("conversationMessages")).toArray());
    out->uiMessages = root.value(QStringLiteral("uiMessages")).toArray().toVariantList();
    out->nextMessageId = root.value(QStringLiteral("nextMessageId")).toInt(0);
    return true;
}

bool save(const Session &session)
{
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("summaryText"), session.summaryText);
    root.insert(QStringLiteral("conversationMessages"),
                ConversationHistory::stripImagePayloads(session.conversationMessages));
    root.insert(QStringLiteral("uiMessages"), QJsonArray::fromVariantList(session.uiMessages));
    root.insert(QStringLiteral("nextMessageId"), session.nextMessageId);

    QFile file(filePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    return true;
}

void clear()
{
    QFile::remove(filePath());
}

} // namespace ChatSessionStore
