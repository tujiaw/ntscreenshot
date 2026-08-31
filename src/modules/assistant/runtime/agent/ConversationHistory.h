#pragma once

#include <QJsonArray>
#include <QString>
#include <QStringList>

namespace ConversationHistory {

constexpr int kDefaultMaxPromptTokens = 24000;

struct TrimResult {
    QJsonArray kept;
    QStringList droppedChunks;
};

int estimateTokens(const QJsonArray &messages);
QString messageToSummaryChunk(const QJsonObject &message);
QJsonArray stripImagePayloads(const QJsonArray &messages);
TrimResult trimToTokenBudget(const QJsonArray &messages,
                             int maxPromptTokens = kDefaultMaxPromptTokens,
                             int lastPromptTokens = -1);

} // namespace ConversationHistory
