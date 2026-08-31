#include "PythonExecutor.h"

#include "LlmToolUtils.h"
#include "ToolAbort.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#include <QStandardPaths>

namespace {

constexpr int kMaxOutputChars = 20 * 1024;
constexpr int kProbeTimeoutMs = 3000;
constexpr auto kWorkerResourcePath = ":/scripts/python_tool_worker.py";
constexpr auto kWorkerFileName = "ntscreenshot_python_tool_worker.py";

QString emptyDisplayText(const QString &text)
{
    return text.isEmpty() ? QStringLiteral("[empty]") : text;
}

QString normalizeCapturedText(QString text)
{
    text = LlmTools::stripAnsi(LlmTools::normalizeLineEndings(text)).trimmed();
    return LlmTools::truncateText(text, kMaxOutputChars);
}

QString probePythonProgram(const QString &candidate)
{
    const QString executable = QStandardPaths::findExecutable(candidate);
    if (executable.isEmpty()) {
        return {};
    }

    QProcess process;
    process.start(executable, QStringList{QStringLiteral("--version")});
    if (!process.waitForStarted(kProbeTimeoutMs)) {
        return {};
    }
    if (!process.waitForFinished(kProbeTimeoutMs)) {
        process.kill();
        process.waitForFinished();
        return {};
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return {};
    }

    const QString versionText = QString::fromUtf8(process.readAllStandardOutput())
                                    + QString::fromUtf8(process.readAllStandardError());
    return versionText.contains(QStringLiteral("Python"), Qt::CaseInsensitive) ? executable : QString();
}

} // namespace

namespace LlmTools {

bool PythonExecutor::isPythonAvailable()
{
    return !pythonProgram().isEmpty();
}

QString PythonExecutor::pythonProgram()
{
    static const QString program = []() -> QString {
        const QString python3Path = probePythonProgram(QStringLiteral("python3"));
        if (!python3Path.isEmpty()) {
            return python3Path;
        }

        const QString pythonPath = probePythonProgram(QStringLiteral("python"));
        if (!pythonPath.isEmpty()) {
            return pythonPath;
        }

        return {};
    }();
    return program;
}

QString PythonExecutor::executeCode(const QString &code,
                                    const QString &workingDirectory,
                                    int timeoutMs,
                                    ToolAbort *abort)
{
    const QString program = pythonProgram();
    if (program.isEmpty()) {
        return QStringLiteral(
            "# Python Result\n\n"
            "- Error: 未检测到可用的 Python 解释器，工具未启用");
    }

    const QString workerPath = workerScriptPath();
    if (workerPath.isEmpty()) {
        return QStringLiteral(
            "# Python Result\n\n"
            "- Error: 无法准备 worker.py 脚本");
    }

    QProcess process;
    if (!workingDirectory.isEmpty()) {
        process.setWorkingDirectory(workingDirectory);
    }

    process.start(program, QStringList{workerPath});
    if (!process.waitForStarted()) {
        return QStringLiteral(
            "# Python Result\n\n"
            "- Error: 无法启动 Python 进程");
    }

    process.write(code.toUtf8());
    process.closeWriteChannel();

    if (!waitForProcess(&process, timeoutMs, abort)) {
        if (abort && abort->isAborted()) {
            return QStringLiteral(
                "# Python Result\n\n"
                "- Error: 执行已取消");
        }
        return QStringLiteral(
            "# Python Result\n\n"
            "- Python: `%1`\n"
            "- Working Directory: `%2`\n"
            "- Error: 执行超时")
            .arg(QDir::toNativeSeparators(program),
                 workingDirectory.isEmpty() ? QDir::toNativeSeparators(QDir::currentPath())
                                            : QDir::toNativeSeparators(workingDirectory));
    }

    const QString rawStdout = QString::fromUtf8(process.readAllStandardOutput());
    const QString rawStderr = QString::fromUtf8(process.readAllStandardError());

    QString stdoutText;
    QString stderrText = normalizeCapturedText(rawStderr);
    int exitCode = process.exitCode();
    bool success = exitCode == 0;

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(rawStdout.toUtf8(), &parseError);
    if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
        const QJsonObject object = doc.object();
        stdoutText = normalizeCapturedText(object.value(QStringLiteral("stdout")).toString());
        const QString workerStderr = normalizeCapturedText(object.value(QStringLiteral("stderr")).toString());
        if (!workerStderr.isEmpty()) {
            stderrText = workerStderr;
        }
        exitCode = object.value(QStringLiteral("exit_code")).toInt(exitCode);
        success = object.value(QStringLiteral("success")).toBool(success);
    } else {
        stdoutText = normalizeCapturedText(rawStdout);
        if (stderrText.isEmpty()) {
            stderrText = QStringLiteral("worker 输出不是合法 JSON");
        }
    }

    return QStringLiteral(
        "# Python Result\n\n"
        "- Python: `%1`\n"
        "- Working Directory: `%2`\n"
        "- Success: %3\n"
        "- Exit Code: %4\n\n"
        "## Stdout\n\n"
        "```text\n%5\n```\n\n"
        "## Stderr\n\n"
        "```text\n%6\n```")
        .arg(QDir::toNativeSeparators(program),
             workingDirectory.isEmpty() ? QDir::toNativeSeparators(QDir::currentPath())
                                        : QDir::toNativeSeparators(workingDirectory),
             success ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(exitCode)
        .arg(emptyDisplayText(stdoutText), emptyDisplayText(stderrText));
}

QString PythonExecutor::workerScriptPath()
{
    static const QString path = []() -> QString {
        QFile resourceFile(QString::fromLatin1(kWorkerResourcePath));
        if (!resourceFile.open(QIODevice::ReadOnly)) {
            return {};
        }

        const QString baseDir = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                    .filePath(QStringLiteral("ntscreenshot"));
        QDir dir(baseDir);
        if (!dir.exists() && !QDir().mkpath(baseDir)) {
            return {};
        }

        const QString workerPath = dir.filePath(QString::fromLatin1(kWorkerFileName));
        QFile workerFile(workerPath);
        if (workerFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            workerFile.write(resourceFile.readAll());
            workerFile.close();
        } else {
            return {};
        }

        return workerPath;
    }();

    return path;
}

} // namespace LlmTools
