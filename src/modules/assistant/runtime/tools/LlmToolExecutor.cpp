#include "LlmToolExecutor.h"

#include "EditFileTool.h"
#include "ExecutePythonTool.h"
#include "FetchUrlTool.h"
#include "ListDirectoryTool.h"
#include "LlmToolUtils.h"
#include "PythonExecutor.h"
#include "ReadFileTool.h"
#include "RunPowerShellTool.h"
#include "RunShellTool.h"
#include "WriteFileTool.h"

#include <QJsonArray>
#include <QList>

namespace {

const QList<const LlmTools::LlmTool *> &registeredTools()
{
    static const LlmTools::FetchUrlTool fetchUrlTool;
    static const LlmTools::ListDirectoryTool listDirectoryTool;
    static const LlmTools::ReadFileTool readFileTool;
    static const LlmTools::WriteFileTool writeFileTool;
    static const LlmTools::EditFileTool editFileTool;
    static const LlmTools::RunPowerShellTool runPowerShellTool;
    static const LlmTools::RunShellTool runShellTool;
    static const LlmTools::ExecutePythonTool executePythonTool;
    static const QList<const LlmTools::LlmTool *> tools = [] {
        QList<const LlmTools::LlmTool *> registered{
            &fetchUrlTool,
            &listDirectoryTool,
            &readFileTool,
            &writeFileTool,
            &editFileTool
        };
#if defined(Q_OS_WIN)
        registered.append(&runPowerShellTool);
#elif defined(Q_OS_LINUX)
        registered.append(&runShellTool);
#endif
        if (LlmTools::PythonExecutor::isPythonAvailable()) {
            registered.append(&executePythonTool);
        }
        return registered;
    }();
    return tools;
}

const LlmTools::LlmTool *findTool(const QString &name)
{
    for (const LlmTools::LlmTool *tool : registeredTools()) {
        if (tool && tool->name() == name) {
            return tool;
        }
    }
    return nullptr;
}

QString unknownToolError(const QString &name)
{
    return QStringLiteral(
        "# Tool Error\n\n"
        "- Tool: `%1`\n"
        "- Error: 未找到对应工具")
        .arg(name);
}

} // namespace

namespace LlmTools {

QJsonArray definitions()
{
    QJsonArray tools;
    for (const LlmTool *tool : registeredTools()) {
        if (tool) {
            tools.append(tool->definition());
        }
    }
    return tools;
}

QString execute(const QString &name, const QString &argumentsJson)
{
    const LlmTool *tool = findTool(name);
    if (!tool) {
        return unknownToolError(name);
    }

    QString errorText;
    const QJsonObject arguments = parseArgumentsJson(argumentsJson, &errorText);
    if (!errorText.isEmpty()) {
        return QStringLiteral(
            "# Tool Error\n\n"
            "- Tool: `%1`\n"
            "- Error: %2")
            .arg(name, errorText);
    }

    return tool->execute(arguments);
}

} // namespace LlmTools
