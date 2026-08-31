#include "LlmTool.h"
#include "ToolAbort.h"
#include "EditFileTool.h"
#include "ExecutePythonTool.h"
#include "FetchUrlTool.h"
#include "ListDirectoryTool.h"
#include "WebSearchTool.h"
#include "PythonExecutor.h"
#include "ReadFileTool.h"
#include "RunPowerShellTool.h"
#include "RunShellTool.h"
#include "WriteFileTool.h"
#include <QSet>

namespace LlmTools {

LlmTool::LlmTool(bool disabled)
    : disabled_(disabled)
{
}

QJsonObject LlmTool::definition() const
{
    QJsonObject functionObject;
    functionObject.insert(QStringLiteral("name"), name());
    functionObject.insert(QStringLiteral("description"), description());
    functionObject.insert(QStringLiteral("parameters"), parameters());

    QJsonObject toolObject;
    toolObject.insert(QStringLiteral("type"), QStringLiteral("function"));
    toolObject.insert(QStringLiteral("function"), functionObject);
    return toolObject;
}

bool LlmTool::disabled() const
{
    return disabled_;
}

void LlmTool::setDisabled(bool disabled)
{
    disabled_ = disabled;
}

QString LlmTool::execute(const QJsonObject &arguments, ToolAbort *abort) const
{
    Q_UNUSED(abort);
    return execute(arguments);
}

bool LlmTool::requiresConfirmation() const
{
    return false;
}

QString LlmTool::argumentError(const QString &message) const
{
    return QStringLiteral(
        "# Tool Error\n\n"
        "- Tool: `%1`\n"
        "- Error: %2")
        .arg(name(), message);
}

QList<QSharedPointer<LlmTool>> createBuiltinTools(SettingModel* settings,
                                                  const QStringList &disabledToolNames,
                                                  bool includeWebSearch)
{
    const QSet<QString> disabledSet(disabledToolNames.begin(), disabledToolNames.end());
    QList<QSharedPointer<LlmTool>> tools;
    tools.reserve(8);

    auto appendTool = [&](LlmTool *tool) {
        if (!tool) {
            return;
        }
        tool->setDisabled(disabledSet.contains(tool->name()));
        tools.append(QSharedPointer<LlmTool>(tool));
    };

    appendTool(new FetchUrlTool());
    if (includeWebSearch) {
        appendTool(new WebSearchTool(settings));
    }
    appendTool(new ListDirectoryTool());
    appendTool(new ReadFileTool());
    appendTool(new WriteFileTool());
    appendTool(new EditFileTool());
    if (PythonExecutor::isPythonAvailable()) {
        appendTool(new ExecutePythonTool());
    }
#if defined(Q_OS_WIN)
    appendTool(new RunPowerShellTool());
#elif defined(Q_OS_LINUX)
    appendTool(new RunShellTool());
#endif

    return tools;
}

QList<ToolMeta> builtinToolMetas(bool includeWebSearch)
{
    QList<ToolMeta> metas;
    const QList<QSharedPointer<LlmTool>> tools = createBuiltinTools(nullptr, {}, includeWebSearch);
    metas.reserve(tools.size());
    for (const auto &tool : tools) {
        if (!tool) {
            continue;
        }
        metas.append({tool->name(), tool->description()});
    }
    return metas;
}

} // namespace LlmTools
