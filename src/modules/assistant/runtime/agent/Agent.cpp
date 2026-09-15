#include "Agent.h"
#include "core/settings/SettingModel.h"
#include "modules/assistant/runtime/agent/OpenAI.h"
#include "modules/assistant/runtime/tools/ToolAbort.h"

#include <QDebug>
#include <QJsonArray>
#include <QMetaObject>
#include <QMutexLocker>

namespace {

const char* KEY_ROLE = "role";
const char* KEY_CONTENT = "content";
const char* KEY_TOOL_CALLS = "tool_calls";
const char* KEY_TOOL_CALL_ID = "tool_call_id";
const char* KEY_FUNCTION = "function";
const char* KEY_NAME = "name";
const char* KEY_ARGUMENTS = "arguments";
const char* KEY_ID = "id";
const char* KEY_TYPE = "type";
const char* KEY_REASONING_CONTENT = "reasoning_content";

QList<Agent::ToolCall> parseToolCalls(const QJsonArray &toolCalls)
{
    QList<Agent::ToolCall> calls;
    for (const QJsonValue &val : toolCalls) {
        const QJsonObject tc = val.toObject();
        const QJsonObject func = tc.value(KEY_FUNCTION).toObject();
        calls.append({
            tc.value(KEY_ID).toString(),
            func.value(KEY_NAME).toString(),
            func.value(KEY_ARGUMENTS).toString()
        });
    }
    return calls;
}

} // namespace

namespace Agent {

AgentRunner::AgentRunner(SettingModel* settings, QObject *parent)
    : QObject(parent)
    , settings_(settings)
{
    workerThread_ = new QThread(this);
    workerThread_->start();

    workerContext_ = new QObject;
    workerContext_->moveToThread(workerThread_);

    connect(this, &AgentRunner::sigDoRun, workerContext_, [this](const QString &msg) {
        doRun(msg);
    });
    connect(this, &AgentRunner::sigDoRunWithImages, workerContext_,
            [this](const QString &text, const QList<QPixmap> &images) {
        doRunWithImages(text, images);
    });
    connect(this, &AgentRunner::sigDoRetry, workerContext_, [this]() {
        doRetry();
    });

    qDebug() << "[Agent] AgentRunner constructed, worker thread started";
}

AgentRunner::~AgentRunner()
{
    qDebug() << "[Agent] AgentRunner destroying...";

    if (chat_) {
        chat_->disconnect(this);
        chat_->disconnect(workerContext_);
    }
    this->disconnect();

    stopped_ = true;
    running_ = false;
    if (toolAbort_) {
        toolAbort_->abort();
    }

    if (chat_) {
        QMetaObject::invokeMethod(chat_, [this]() {
            chat_->abortActiveReply();
        }, Qt::BlockingQueuedConnection);
    }

    OpenAIChat *chat = chat_;
    chat_ = nullptr;
    if (chat) {
        if (chat->thread() == QThread::currentThread()) {
            delete chat;
        } else {
            QMetaObject::invokeMethod(chat, [chat]() {
                delete chat;
            }, Qt::BlockingQueuedConnection);
        }
    }

    QObject *workerCtx = workerContext_;
    workerContext_ = nullptr;
    if (workerCtx) {
        if (workerCtx->thread() == QThread::currentThread()) {
            delete workerCtx;
        } else {
            QMetaObject::invokeMethod(workerCtx, [workerCtx]() {
                delete workerCtx;
            }, Qt::BlockingQueuedConnection);
        }
    }

    workerThread_->quit();
    if (!workerThread_->wait(3000)) {
        qDebug() << "[Agent] WARNING: worker thread did not finish in 3s, terminating";
        workerThread_->terminate();
        workerThread_->wait();
    }

    qDebug() << "[Agent] AgentRunner destroyed";
}

void AgentRunner::setToolRegistry(ToolRegistry *registry)
{
    registry_ = registry;
    if (registry_) {
        qDebug() << "[Agent] ToolRegistry set, available tools:" << registry_->toolNames();
    } else {
        qDebug() << "[Agent] ToolRegistry cleared";
    }

    if (chat_) {
        const QJsonArray definitions = registry_ ? registry_->definitions() : QJsonArray();
        QMetaObject::invokeMethod(chat_, [this, definitions]() {
            if (chat_) {
                chat_->setToolDefinitions(definitions);
            }
        }, Qt::BlockingQueuedConnection);
    }
}

void AgentRunner::setMaxIterations(int max)
{
    maxIterations_ = qMax(1, max);
    qDebug() << "[Agent] Max iterations set to:" << maxIterations_;
}

void AgentRunner::setupChat()
{
    if (chat_) {
        QMetaObject::invokeMethod(chat_, [this]() {
            if (registry_) {
                chat_->setToolDefinitions(registry_->definitions());
            }
        }, Qt::BlockingQueuedConnection);
        return;
    }

    chat_ = new OpenAIChat(settings_, nullptr);
    chat_->moveToThread(workerThread_);
    chat_->setStreamingEnabled(true);

    if (registry_) {
        chat_->setToolDefinitions(registry_->definitions());
        qDebug() << "[Agent] OpenAIChat created (streaming=true, tools=" << registry_->toolNames() << ")";
    } else {
        qDebug() << "[Agent] OpenAIChat created (streaming=true, no tools)";
    }

    connect(chat_, &OpenAIChat::sigStreamFinished, workerContext_, [this]() {
        syncSessionCache();
    });
    connect(chat_, &OpenAIChat::sigResponse, workerContext_, [this]() {
        syncSessionCache();
    });
    connect(chat_, &OpenAIChat::sigError, workerContext_, [this]() {
        syncSessionCache();
    });
    connect(chat_, &OpenAIChat::sigStreamStarted, this, &AgentRunner::sigStreamStarted);
    connect(chat_, &OpenAIChat::sigStreamDelta, this, &AgentRunner::sigStreamDelta);
    connect(chat_, &OpenAIChat::sigUsageAvailable, this, &AgentRunner::sigUsageAvailable);
    connect(chat_, &OpenAIChat::sigStreamFinished, this, [this](const QString &text) {
        if (phase_.load() == Phase::ExecutingTools) {
            return;
        }
        running_ = false;
        phase_.store(Phase::Idle);
        qDebug() << "[Agent] Stream finished, length:" << text.length();
        emit sigStreamFinished(text);
        emit sigStateChanged(false);
        emit sigSessionChanged();
    });
    connect(chat_, &OpenAIChat::sigToolCallsReceived, this,
            [this](const QJsonArray &toolCalls, const QString &textContent, const QString &reasoningContent) {
        Q_UNUSED(toolCalls);
        Q_UNUSED(reasoningContent);
        emit sigAssistantPrefixFinalized(textContent);
    });
    connect(chat_, &OpenAIChat::sigToolCallsReceived, workerContext_,
            [this](const QJsonArray &toolCalls, const QString &textContent, const QString &reasoningContent) {
        handleWorkerToolCalls(toolCalls, textContent, reasoningContent);
    }, Qt::QueuedConnection);
    connect(chat_, &OpenAIChat::sigResponse, this, [this](const QString &text) {
        if (phase_.load() == Phase::ExecutingTools) {
            return;
        }
        running_ = false;
        phase_.store(Phase::Idle);
        qDebug() << "[Agent] Response received (non-streaming), length:" << text.length();
        emit sigStreamFinished(text);
        emit sigStateChanged(false);
        emit sigSessionChanged();
    });
    connect(chat_, &OpenAIChat::sigError, this, [this](const QString &text) {
        running_ = false;
        phase_.store(Phase::Idle);
        qDebug() << "[Agent] Error from OpenAIChat:" << text.left(200);
        emit sigError(text);
        emit sigStateChanged(false);
        emit sigSessionChanged();
    });
}

void AgentRunner::run(const QString &userMessage)
{
    if (running_) {
        qDebug() << "[Agent] run() ignored — already running";
        return;
    }

    setupChat();
    running_ = true;
    stopped_ = false;
    currentIteration_ = 0;
    emit sigStateChanged(true);

    qDebug() << "[Agent] >>> Agent started (text), max iterations:" << maxIterations_;
    emit sigDoRun(userMessage);
}

void AgentRunner::runWithImages(const QString &text, const QList<QPixmap> &images)
{
    if (running_) {
        qDebug() << "[Agent] runWithImages() ignored — already running";
        return;
    }

    setupChat();
    running_ = true;
    stopped_ = false;
    currentIteration_ = 0;
    emit sigStateChanged(true);

    qDebug() << "[Agent] >>> Agent started (images:" << images.size()
             << "), max iterations:" << maxIterations_;
    emit sigDoRunWithImages(text, images);
}

void AgentRunner::retryLastResponse()
{
    if (running_ || !chat_) return;

    running_ = true;
    stopped_ = false;
    currentIteration_ = 0;
    emit sigStateChanged(true);

    qDebug() << "[Agent] >>> Agent retry requested, max iterations:" << maxIterations_;
    emit sigDoRetry();
}

void AgentRunner::resetConversation()
{
    stopped_ = true;
    running_ = false;
    pendingCalls_.clear();
    pendingCallIndex_ = 0;
    phase_.store(Phase::Idle);
    if (toolAbort_) {
        toolAbort_->abort();
    }

    if (chat_) {
        QMetaObject::invokeMethod(chat_, [this]() {
            chat_->resetConversation();
            syncSessionCache();
        }, Qt::BlockingQueuedConnection);
    }

    currentIteration_ = 0;
    pendingTextContent_.clear();
    pendingReasoningContent_.clear();
    emit sigStateChanged(false);
    emit sigSessionChanged();
}

void AgentRunner::stop()
{
    if (!running_) return;
    qDebug() << "[Agent] Stop requested (iteration" << currentIteration_ << "/" << maxIterations_ << ")";
    stopped_ = true;
    running_ = false;
    phase_.store(Phase::Idle);
    if (toolAbort_) {
        toolAbort_->abort();
    }
    if (chat_) {
        QMetaObject::invokeMethod(chat_, [this]() {
            chat_->abortActiveReply();
        }, Qt::QueuedConnection);
    }
    emit sigStateChanged(false);
}

bool AgentRunner::isRunning() const
{
    return running_;
}

QJsonArray AgentRunner::conversationSnapshot() const
{
    QMutexLocker locker(&sessionMutex_);
    return conversationCache_;
}

QString AgentRunner::summarySnapshot() const
{
    QMutexLocker locker(&sessionMutex_);
    return summaryCache_;
}

void AgentRunner::syncSessionCache()
{
    if (!chat_) {
        QMutexLocker locker(&sessionMutex_);
        conversationCache_ = QJsonArray();
        summaryCache_.clear();
        return;
    }
    QMutexLocker locker(&sessionMutex_);
    conversationCache_ = chat_->conversationMessages();
    summaryCache_ = chat_->summaryText();
}

void AgentRunner::restoreSession(const QJsonArray &messages, const QString &summaryText)
{
    setupChat();
    QMetaObject::invokeMethod(chat_, [this, messages, summaryText]() {
        if (chat_) {
            chat_->restoreSession(messages, summaryText);
            syncSessionCache();
        }
    }, Qt::BlockingQueuedConnection);
}

void AgentRunner::doRun(const QString &userMessage)
{
    qDebug() << "[Agent] Worker: doRun";
    currentIteration_ = 1;
    phase_.store(Phase::WaitingLlm);
    emit sigIterationChanged(currentIteration_, maxIterations_);
    chat_->sendMessage(userMessage);
}

void AgentRunner::doRunWithImages(const QString &text, const QList<QPixmap> &images)
{
    qDebug() << "[Agent] Worker: doRunWithImages — images:" << images.size();
    currentIteration_ = 1;
    phase_.store(Phase::WaitingLlm);
    emit sigIterationChanged(currentIteration_, maxIterations_);
    chat_->sendImages(text, images);
}

void AgentRunner::doRetry()
{
    qDebug() << "[Agent] Worker: doRetry — resending last conversation";
    currentIteration_ = 1;
    phase_.store(Phase::WaitingLlm);
    emit sigIterationChanged(currentIteration_, maxIterations_);
    chat_->retryLastResponse();
}

void AgentRunner::handleWorkerToolCalls(const QJsonArray &toolCalls, const QString &textContent,
                                       const QString &reasoningContent)
{
    if (stopped_) {
        finishWorkerRun();
        return;
    }

    qDebug() << "[Agent] LLM returned" << toolCalls.size() << "tool call(s)";
    pendingTextContent_ = textContent;
    pendingReasoningContent_ = reasoningContent;
    beginToolBatch(parseToolCalls(toolCalls));
}

void AgentRunner::beginToolBatch(const QList<ToolCall> &calls)
{
    phase_.store(Phase::ExecutingTools);
    pendingCalls_ = calls;
    pendingCallIndex_ = 0;

    QJsonArray toolCallsJson;
    for (const ToolCall &call : calls) {
        QJsonObject funcObj;
        funcObj[KEY_NAME] = call.name;
        funcObj[KEY_ARGUMENTS] = call.arguments;

        QJsonObject tcObj;
        tcObj[KEY_ID] = call.id;
        tcObj[KEY_TYPE] = QStringLiteral("function");
        tcObj[KEY_FUNCTION] = funcObj;
        toolCallsJson.append(tcObj);
    }

    QJsonObject assistantMsg;
    assistantMsg[KEY_ROLE] = QStringLiteral("assistant");
    assistantMsg[KEY_CONTENT] = pendingTextContent_.isEmpty()
        ? QJsonValue(QJsonValue::Null)
        : QJsonValue(pendingTextContent_);
    assistantMsg[KEY_TOOL_CALLS] = toolCallsJson;
    if (!pendingReasoningContent_.isEmpty()) {
        assistantMsg[KEY_REASONING_CONTENT] = pendingReasoningContent_;
    }
    chat_->appendConversationMessage(assistantMsg);
    syncSessionCache();
    processNextPendingTool();
}

void AgentRunner::processNextPendingTool()
{
    if (stopped_) {
        while (pendingCallIndex_ < pendingCalls_.size() && chat_) {
            const ToolCall call = pendingCalls_.at(pendingCallIndex_);
            QJsonObject toolMsg;
            toolMsg[KEY_ROLE] = QStringLiteral("tool");
            toolMsg[KEY_TOOL_CALL_ID] = call.id;
            toolMsg[KEY_CONTENT] = QStringLiteral(
                "# Tool Error\n\n- Tool: `%1`\n- Error: 执行已取消").arg(call.name);
            chat_->appendConversationMessage(toolMsg);
            ++pendingCallIndex_;
        }
        syncSessionCache();
        finishWorkerRun();
        return;
    }
    if (pendingCallIndex_ >= pendingCalls_.size()) {
        afterAllTools();
        return;
    }

    const ToolCall &call = pendingCalls_.at(pendingCallIndex_);
    QMetaObject::invokeMethod(this, [this, call]() {
        emit sigToolExecuting(call.name, call.arguments);
    });

    executeCurrentTool();
}

void AgentRunner::executeCurrentTool()
{
    if (pendingCallIndex_ >= pendingCalls_.size()) {
        afterAllTools();
        return;
    }

    const ToolCall call = pendingCalls_.at(pendingCallIndex_);
    QString result;
    QString fullResult;
    if (!registry_ || !registry_->hasTool(call.name)) {
        result = QStringLiteral("# Tool Error\n\n- Tool: `%1`\n- Error: no matching tool in registry").arg(call.name);
        fullResult = result;
    } else {
        toolAbort_ = QSharedPointer<LlmTools::ToolAbort>::create();
        result = registry_->execute(call.name, call.arguments, toolAbort_.data(), &fullResult);
        toolAbort_.reset();
    }

    QMetaObject::invokeMethod(this, [this, call, fullResult]() {
        emit sigToolExecuted(call.name, fullResult);
    });

    QJsonObject toolMsg;
    toolMsg[KEY_ROLE] = QStringLiteral("tool");
    toolMsg[KEY_TOOL_CALL_ID] = call.id;
    toolMsg[KEY_CONTENT] = result;
    chat_->appendConversationMessage(toolMsg);
    syncSessionCache();

    ++pendingCallIndex_;
    processNextPendingTool();
}

void AgentRunner::afterAllTools()
{
    if (stopped_) {
        finishWorkerRun();
        return;
    }

    if (currentIteration_ >= maxIterations_) {
        qDebug() << "[Agent] Max iterations reached after tool processing — final answer without tools";
        runFinalAnswerWithoutTools();
        return;
    }

    ++currentIteration_;
    phase_.store(Phase::WaitingLlm);
    emit sigIterationChanged(currentIteration_, maxIterations_);
    if (registry_) {
        chat_->setToolDefinitions(registry_->definitions());
    }
    qDebug() << "[Agent] Posting conversation with tool results, continuing to next iteration";
    chat_->postConversationAsync();
}

void AgentRunner::runFinalAnswerWithoutTools()
{
    if (stopped_) {
        finishWorkerRun();
        return;
    }

    chat_->setToolDefinitions(QJsonArray());
    phase_.store(Phase::WaitingLlm);
    qDebug() << "[Agent] Final answer without tools — posting once, no tool definitions";
    chat_->postConversationAsync();
}

void AgentRunner::finishWorkerRun()
{
    phase_.store(Phase::Idle);
    pendingCalls_.clear();
    QMetaObject::invokeMethod(this, [this]() {
        running_ = false;
        emit sigStateChanged(false);
        emit sigSessionChanged();
    });
}

} // namespace Agent
