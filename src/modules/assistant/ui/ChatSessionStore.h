#pragma once

#include <QJsonArray>
#include <QString>
#include <QVariantList>

namespace ChatSessionStore {

struct Session {
    QJsonArray conversationMessages;
    QString summaryText;
    QVariantList uiMessages;
    int nextMessageId = 0;
};

QString filePath();
bool load(Session *out);
bool save(const Session &session);
void clear();

} // namespace ChatSessionStore
