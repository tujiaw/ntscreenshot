#include "RunPowerShellTool.h"

#include "LlmToolUtils.h"
#include "ToolAbort.h"

#include <QDir>
#include <QJsonArray>
#include <QProcess>

namespace {

constexpr int kDefaultTimeoutMs = 15 * 1000;
constexpr int kMaxTimeoutMs = 60 * 1000;

} // namespace

namespace LlmTools {

QString RunPowerShellTool::name() const
{
    return QStringLiteral("run_powershell");
}

QString RunPowerShellTool::description() const
{
    return QStringLiteral("在 Windows 上执行 PowerShell 命令，并返回 stdout、stderr 和退出码。");
}

QJsonObject RunPowerShellTool::parameters() const
{
    QJsonObject parametersObject;
    parametersObject.insert(QStringLiteral("type"), QStringLiteral("object"));
    parametersObject.insert(QStringLiteral("properties"), QJsonObject{
        {QStringLiteral("command"), QJsonObject{
            {QStringLiteral("type"), QStringLiteral("string")},
            {QStringLiteral("description"), QStringLiteral("要执行的 Windows PowerShell 命令")}
        }},
        {QStringLiteral("working_directory"), QJsonObject{
            {QStringLiteral("type"), QStringLiteral("string")},
            {QStringLiteral("description"), QStringLiteral("可选，PowerShell 命令的工作目录")}
        }},
        {QStringLiteral("timeout_ms"), QJsonObject{
            {QStringLiteral("type"), QStringLiteral("integer")},
            {QStringLiteral("description"), QStringLiteral("可选，执行超时时间，单位毫秒，默认 15000")}
        }}
    });
    parametersObject.insert(QStringLiteral("required"), QJsonArray{QStringLiteral("command")});
    parametersObject.insert(QStringLiteral("additionalProperties"), false);
    return parametersObject;
}

QString RunPowerShellTool::execute(const QJsonObject &arguments) const
{
    return execute(arguments, nullptr);
}

QString RunPowerShellTool::execute(const QJsonObject &arguments, ToolAbort *abort) const
{
    const QString command = arguments.value(QStringLiteral("command")).toString().trimmed();
    if (command.isEmpty()) {
        return argumentError(QStringLiteral("缺少 command 参数"));
    }

#ifndef Q_OS_WIN
    Q_UNUSED(arguments);
    Q_UNUSED(abort);
    return QStringLiteral(
        "# PowerShell Result\n\n"
        "- Error: 当前平台不是 Windows，无法执行 PowerShell 命令");
#else
    const QString workingDirectory = arguments.value(QStringLiteral("working_directory")).toString().trimmed();
    const int timeoutMs = clampInt(arguments.value(QStringLiteral("timeout_ms")).toInt(kDefaultTimeoutMs),
                                   1000,
                                   kMaxTimeoutMs);

    QProcess process;
    if (!workingDirectory.isEmpty()) {
        process.setWorkingDirectory(workingDirectory);
    }

    const QStringList processArguments{
        QStringLiteral("-NoLogo"),
        QStringLiteral("-NoProfile"),
        QStringLiteral("-NonInteractive"),
        QStringLiteral("-ExecutionPolicy"),
        QStringLiteral("Bypass"),
        QStringLiteral("-Command"),
        command
    };

    process.start(QStringLiteral("powershell.exe"), processArguments);
    if (!process.waitForStarted()) {
        return QStringLiteral(
            "# PowerShell Result\n\n"
            "- Command: `%1`\n"
            "- Error: 无法启动 PowerShell")
            .arg(command);
    }

    if (!waitForProcess(&process, timeoutMs, abort)) {
        if (abort && abort->isAborted()) {
            return QStringLiteral(
                "# PowerShell Result\n\n"
                "- Command: `%1`\n"
                "- Error: 执行已取消")
                .arg(command);
        }
        return QStringLiteral(
            "# PowerShell Result\n\n"
            "- Command: `%1`\n"
            "- Working Directory: `%2`\n"
            "- Error: 执行超时")
            .arg(command,
                 workingDirectory.isEmpty() ? QDir::toNativeSeparators(QDir::currentPath())
                                            : QDir::toNativeSeparators(workingDirectory));
    }

    QString stdoutText = QString::fromLocal8Bit(process.readAllStandardOutput());
    QString stderrText = QString::fromLocal8Bit(process.readAllStandardError());
    stdoutText = truncateText(stripAnsi(normalizeLineEndings(stdoutText)).trimmed(), kToolOutputMaxChars);
    stderrText = truncateText(stripAnsi(normalizeLineEndings(stderrText)).trimmed(), kToolOutputMaxChars);

    if (stdoutText.isEmpty()) {
        stdoutText = QStringLiteral("[empty]");
    }
    if (stderrText.isEmpty()) {
        stderrText = QStringLiteral("[empty]");
    }

    return QStringLiteral(
        "# PowerShell Result\n\n"
        "- Command: `%1`\n"
        "- Working Directory: `%2`\n"
        "- Exit Code: %3\n\n"
        "## Stdout\n\n"
        "```text\n%4\n```\n\n"
        "## Stderr\n\n"
        "```text\n%5\n```")
        .arg(command,
             workingDirectory.isEmpty() ? QDir::toNativeSeparators(QDir::currentPath())
                                        : QDir::toNativeSeparators(workingDirectory))
        .arg(process.exitCode())
        .arg(stdoutText, stderrText);
#endif
}

} // namespace LlmTools
