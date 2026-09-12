#pragma once

#include <QList>
#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QQueue>
#include <QTimer>
#include "AgentContext.h"

class QNetworkAccessManager;
class QNetworkReply;
class QPixmap;
class SettingModel;

class OpenAIChat : public QObject {
    Q_OBJECT

public:
    explicit OpenAIChat(SettingModel* settings, QObject *parent = nullptr);
    void resetConversation();
    void retryLastResponse();
    void sendMessage(const QString &message);
    void sendImages(const QString &text, const QList<QPixmap> &images);
    void sendImage(const QString &text, const QPixmap &image);

    // --- 对话控制 ---
    void setStreamingEnabled(bool enabled);
    void setToolDefinitions(const QJsonArray &definitions);
    void appendConversationMessage(const QJsonObject &message);
    void restoreSession(const QJsonArray &messages, const QString &summaryText);
    void postConversationAsync();
    QJsonArray conversationMessages() const;
    QString summaryText() const;
    void abortActiveReply();

signals:
    void sigResponse(const QString &text);
    void sigStreamStarted();
    void sigStreamDelta(const QString &text);
    void sigStreamFinished(const QString &text);
    void sigUsageAvailable(int inputTokens, int outputTokens, int totalTokens);
    void sigError(const QString &text);
    void sigRequestStateChanged(bool pending);
    void sigToolCallsReceived(const QJsonArray &toolCalls, const QString &textContent,
                              const QString &reasoningContent);

private slots:
    void handleReplyReadyRead();
    void handleReplyFinished();
    void handleActiveReplyTimeout();
    void handleStreamIdleTimeout();
    void onSummaryReadyRead();
    void onSummaryFinished();

private:
    SettingModel* settings_ = nullptr;
    struct DroppedRound {
        QString mergedDialog;
    };

    void postConversation();
    void resetActiveReplyState();
    void finalizeSuccessfulReply(const QString &text, const QString &reasoningContent = QString());
    void trimHistory();
    void processNextDropped();
    void postSummaryRequest(const DroppedRound &round);
    void prepareForNewTurn();
    void handleNonStreamingReply(const QByteArray &responseData);
    void retryAfterRateLimit();
    void emitUsageIfAvailable();

    QNetworkAccessManager *manager_;
    QJsonArray conversationMessages_;
    bool requestPending_ = false;
    QNetworkReply *activeReply_ = nullptr;
    QTimer *activeReplyTimeoutTimer_ = nullptr;
    QTimer *streamIdleTimer_ = nullptr;
    QString activeReplyTimeoutReason_;
    QByteArray replyBuffer_;
    QByteArray streamBuffer_;
    QString streamingText_;
    QString streamingReasoningText_;
    bool streamStarted_ = false;
    bool streamCompleted_ = false;
    int lastInputTokens_ = -1;
    int lastOutputTokens_ = -1;
    int lastTotalTokens_ = -1;

    // 异步滚动摘要
    QString summaryText_;
    bool summaryPending_ = false;
    QNetworkReply *summaryReply_ = nullptr;
    QByteArray summaryBuffer_;
    QQueue<DroppedRound> droppedQueue_;

    // 可选配置
    bool streamingEnabled_ = false;
    QJsonArray toolDefinitions_;
    Agent::ToolCallAccumulator streamToolCallAccumulator_;

    // Rate limit retry
    int rateLimitRetryCount_ = 0;
    static constexpr int kMaxRateLimitRetries = 3;

};
