#include "ExecutePythonTool.h"

#include "LlmToolUtils.h"
#include "PythonExecutor.h"
#include "ToolAbort.h"

#include <QJsonArray>

namespace {

constexpr int kDefaultTimeoutMs = 15 * 1000;
constexpr int kMaxTimeoutMs = 60 * 1000;

} // namespace

namespace LlmTools {

QString ExecutePythonTool::name() const
{
    return QStringLiteral("execute_python");
}

QString ExecutePythonTool::description() const
{
    return QStringLiteral("执行一段 Python 代码并返回运行结果。适合用于计算、数据处理、文本处理和脚本验证；输入为完整的 Python 代码，输出包含 stdout、stderr、是否成功和退出码。");
}

QJsonObject ExecutePythonTool::parameters() const
{
    QJsonObject parametersObject;
    parametersObject.insert(QStringLiteral("type"), QStringLiteral("object"));
    parametersObject.insert(QStringLiteral("properties"), QJsonObject{
        {QStringLiteral("code"), QJsonObject{
            {QStringLiteral("type"), QStringLiteral("string")},
            {QStringLiteral("description"), QStringLiteral("要执行的 Python 代码")}
        }},
        {QStringLiteral("working_directory"), QJsonObject{
            {QStringLiteral("type"), QStringLiteral("string")},
            {QStringLiteral("description"), QStringLiteral("可选，Python 代码执行时的工作目录")}
        }},
        {QStringLiteral("timeout_ms"), QJsonObject{
            {QStringLiteral("type"), QStringLiteral("integer")},
            {QStringLiteral("description"), QStringLiteral("可选，执行超时时间，单位毫秒，默认 15000")}
        }}
    });
    parametersObject.insert(QStringLiteral("required"), QJsonArray{QStringLiteral("code")});
    parametersObject.insert(QStringLiteral("additionalProperties"), false);
    return parametersObject;
}

QString ExecutePythonTool::execute(const QJsonObject &arguments) const
{
    return execute(arguments, nullptr);
}

QString ExecutePythonTool::execute(const QJsonObject &arguments, ToolAbort *abort) const
{
    const QString code = arguments.value(QStringLiteral("code")).toString();
    if (code.trimmed().isEmpty()) {
        return argumentError(QStringLiteral("缺少 code 参数"));
    }

    const QString workingDirectory = arguments.value(QStringLiteral("working_directory")).toString().trimmed();
    const int timeoutMs = clampInt(arguments.value(QStringLiteral("timeout_ms")).toInt(kDefaultTimeoutMs),
                                   1000,
                                   kMaxTimeoutMs);

    return PythonExecutor::executeCode(code, workingDirectory, timeoutMs, abort);
}

} // namespace LlmTools
