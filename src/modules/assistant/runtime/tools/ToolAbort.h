#pragma once

#include <QMutex>
#include <QPointer>

class QProcess;

namespace LlmTools {

class ToolAbort {
public:
    bool isAborted() const;
    void abort();
    void registerProcess(QProcess *process);
    void unregisterProcess(QProcess *process);

private:
    mutable QMutex mutex_;
    QPointer<QProcess> process_;
    bool aborted_ = false;
};

} // namespace LlmTools
