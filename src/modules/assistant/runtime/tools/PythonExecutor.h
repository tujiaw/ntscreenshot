#pragma once

#include <QJsonObject>
#include <QString>

namespace LlmTools {

class ToolAbort;

class PythonExecutor {
public:
    static bool isPythonAvailable();
    static QString pythonProgram();
    static QString executeCode(const QString &code,
                               const QString &workingDirectory,
                               int timeoutMs,
                               ToolAbort *abort = nullptr);

private:
    static QString workerScriptPath();
};

} // namespace LlmTools
