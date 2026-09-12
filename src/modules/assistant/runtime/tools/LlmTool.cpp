#include "LlmTool.h"
#include "ToolAbort.h"

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

QString LlmTool::argumentError(const QString &message) const
{
    return QStringLiteral(
        "# Tool Error\n\n"
        "- Tool: `%1`\n"
        "- Error: %2")
        .arg(name(), message);
}

} // namespace LlmTools
