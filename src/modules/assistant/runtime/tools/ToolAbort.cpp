#include "ToolAbort.h"

#include <QProcess>

namespace LlmTools {

bool ToolAbort::isAborted() const
{
    QMutexLocker locker(&mutex_);
    return aborted_;
}

void ToolAbort::abort()
{
    QProcess *process = nullptr;
    {
        QMutexLocker locker(&mutex_);
        aborted_ = true;
        process = process_.data();
    }
    if (process) {
        process->kill();
    }
}

void ToolAbort::registerProcess(QProcess *process)
{
    bool alreadyAborted = false;
    {
        QMutexLocker locker(&mutex_);
        process_ = process;
        alreadyAborted = aborted_;
    }
    if (alreadyAborted && process) {
        process->kill();
    }
}

void ToolAbort::unregisterProcess(QProcess *process)
{
    QMutexLocker locker(&mutex_);
    if (process_.data() == process) {
        process_.clear();
    }
}

} // namespace LlmTools
