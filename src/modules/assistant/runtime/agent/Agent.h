#pragma once

#include <QObject>
#include <QThread>
#include <QJsonArray>
#include <QJsonObject>
#include <QPixmap>
#include <QList>
#include <QSharedPointer>
#include <QMutex>
#include <atomic>
#include "AgentContext.h"
#include "AgentToolRegistry.h"

class OpenAIChat;
class SettingModel;

namespace LlmTools {
class ToolAbort;
}

namespace Agent {

class AgentRunner : public QObject {
    Q_OBJECT

public:
    explicit AgentRunner(SettingModel* settings, QObject *parent = nullptr);
    ~AgentRunner();

    void setToolRegistry(ToolRegistry *registry);
    void setMaxIterations(int max);

    void run(const QString &userMessage);
    void runWithImages(const QString &text, const QList<QPixmap> &images);
    void retryLastResponse();
    void resetConversation();
    void stop();
    bool isRunning() const;

    QJsonArray conversationSnapshot() const;
    QString summarySnapshot() const;
    void restoreSession(const QJsonArray &messages, const QString &summaryText);

signals:
    void sigStreamStarted();
    void sigStreamDelta(const QString &text);
    void sigStreamFinished(const QString &fullText);
    void sigUsageAvailable(int inputTokens, int outputTokens, int totalTokens);
    void sigAssistantPrefixFinalized(const QString &text);
    void sigToolExecuting(const QString &name, const QString &args);
    void sigToolExecuted(const QString &name, const QString &result);
    void sigError(const QString &text);
    void sigStateChanged(bool running);
    void sigIterationChanged(int current, int max);
    void sigSessionChanged();

    void sigDoRun(const QString &userMessage);
    void sigDoRunWithImages(const QString &text, const QList<QPixmap> &images);
    void sigDoRetry();

private:
    enum class Phase {
        Idle,
        WaitingLlm,
        ExecutingTools
    };

    SettingModel* settings_ = nullptr;
    void setupChat();
    void doRun(const QString &userMessage);
    void doRunWithImages(const QString &text, const QList<QPixmap> &images);
    void doRetry();
    void handleWorkerToolCalls(const QJsonArray &toolCalls, const QString &textContent,
                               const QString &reasoningContent);
    void beginToolBatch(const QList<ToolCall> &calls);
    void processNextPendingTool();
    void executeCurrentTool();
    void afterAllTools();
    void runFinalAnswerWithoutTools();
    void finishWorkerRun();
    void syncSessionCache();

    OpenAIChat *chat_ = nullptr;
    ToolRegistry *registry_ = nullptr;
    QThread *workerThread_ = nullptr;
    QObject *workerContext_ = nullptr;
    int maxIterations_ = 10;
    int currentIteration_ = 0;
    bool running_ = false;
    bool stopped_ = false;
    std::atomic<Phase> phase_{Phase::Idle};
    QString pendingTextContent_;
    QString pendingReasoningContent_;
    QList<ToolCall> pendingCalls_;
    int pendingCallIndex_ = 0;
    QSharedPointer<LlmTools::ToolAbort> toolAbort_;
    mutable QMutex sessionMutex_;
    QJsonArray conversationCache_;
    QString summaryCache_;
};

} // namespace Agent
