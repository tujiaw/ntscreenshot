#include "AgentToolRegistry.h"
#include "modules/assistant/runtime/tools/LlmToolUtils.h"
#include "modules/assistant/runtime/tools/ToolAbort.h"

namespace Agent {

ToolRegistry::ToolRegistry(QObject *parent)
    : QObject(parent)
{
}

void ToolRegistry::registerTool(QSharedPointer<LlmTools::LlmTool> tool)
{
    if (!tool) return;
    if (tool->disabled()) return;
    const QString toolName = tool->name();
    tools_.insert(toolName, std::move(tool));
    emit sigToolRegistered(toolName);
}

void ToolRegistry::unregisterTool(const QString &name)
{
    if (tools_.remove(name) > 0) {
        emit sigToolUnregistered(name);
    }
}

bool ToolRegistry::hasTool(const QString &name) const
{
    return tools_.contains(name);
}

QStringList ToolRegistry::toolNames() const
{
    return tools_.keys();
}

QJsonArray ToolRegistry::definitions() const
{
    QJsonArray defs;
    for (auto it = tools_.constBegin(); it != tools_.constEnd(); ++it) {
        if (it.value()) {
            defs.append(it.value()->definition());
        }
    }
    return defs;
}

bool ToolRegistry::requiresConfirmation(const QString &name) const
{
    auto it = tools_.constFind(name);
    if (it == tools_.constEnd() || !it.value()) {
        return false;
    }
    return it.value()->requiresConfirmation();
}

QString ToolRegistry::execute(const QString &name, const QString &argumentsJson,
                              LlmTools::ToolAbort *abort) const
{
    auto it = tools_.constFind(name);
    if (it == tools_.constEnd() || !it.value()) {
        return QStringLiteral("# Tool Error\n\n- Tool: `%1`\n- Error: unregistered tool").arg(name);
    }

    QString errorText;
    const QJsonObject arguments = LlmTools::parseArgumentsJson(argumentsJson, &errorText);
    if (!errorText.isEmpty()) {
        return QStringLiteral("# Tool Error\n\n- Tool: `%1`\n- Error: %2").arg(name, errorText);
    }

    return LlmTools::truncateText(it.value()->execute(arguments, abort), LlmTools::kToolOutputMaxChars);
}

} // namespace Agent
