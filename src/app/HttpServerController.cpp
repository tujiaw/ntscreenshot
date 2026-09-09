#include "HttpServerController.h"

#include <QFileInfo>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QProcess>
#include <QStandardPaths>

namespace {

constexpr int kProbeTimeoutMs = 3000;

QString probePythonProgram(const QString& candidate)
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
    return versionText.contains(QStringLiteral("Python"), Qt::CaseInsensitive)
               ? executable
               : QString();
}

} // namespace

HttpServerController::HttpServerController(QObject* parent)
    : QObject(parent)
{
}

HttpServerController::~HttpServerController()
{
    if (proc_ && running_) {
        proc_->kill();
    }
}

bool HttpServerController::pythonAvailable() const
{
    return !pythonProgram().isEmpty();
}

QString HttpServerController::pythonProgram() const
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

bool HttpServerController::start(const Params& params)
{
    if (running_) {
        return true;
    }
    if (proc_) {
        // A stale process object from a previous run should not exist here, but
        // make sure we are not leaking one.
        proc_->deleteLater();
        proc_ = nullptr;
    }

    lastError_.clear();
    const QString program = pythonProgram();
    if (program.isEmpty()) {
        lastError_ = QStringLiteral("未检测到可用的 Python 解释器，请先安装并加入 PATH。");
        return false;
    }
    const QFileInfo dirInfo(params.directory);
    if (params.directory.isEmpty() || !dirInfo.isDir()) {
        lastError_ = QStringLiteral("共享目录无效或不存在，请重新选择。");
        return false;
    }
    if (params.port < 1 || params.port > 65535) {
        lastError_ = QStringLiteral("端口必须在 1~65535 之间。");
        return false;
    }

    params_ = params;
    running_ = false;
    stopping_ = false;
    stderrBuf_.clear();

    QStringList args{QStringLiteral("-m"), QStringLiteral("http.server"),
                     QStringLiteral("-d"), params_.directory,
                     QStringLiteral("-p"), QString::number(params_.port),
                     QStringLiteral("-b"), params_.bind};
    if (!params_.protocol.isEmpty()) {
        args << QStringLiteral("--protocol") << params_.protocol;
    }
    if (params_.cgi) {
        args << QStringLiteral("--cgi");
    }

    auto* proc = new QProcess(this);
    proc_ = proc;
    proc->setWorkingDirectory(params_.directory);

    connect(proc, &QProcess::started, this, [this]() {
        if (!running_) {
            running_ = true;
            emit runningChanged(true);
        }
    });
    connect(proc, &QProcess::readyReadStandardError, this, [this, proc]() {
        if (proc == proc_) {
            stderrBuf_.append(proc->readAllStandardError());
        }
    });
    connect(proc, &QProcess::errorOccurred, this, [this, proc](QProcess::ProcessError error) {
        if (proc != proc_ || stopping_) {
            return;
        }
        if (error == QProcess::FailedToStart) {
            running_ = false;
            proc_ = nullptr;
            proc->deleteLater();
            emit errorOccurred(QStringLiteral("无法启动 python：%1").arg(proc->errorString()));
        }
    });
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int exitCode, QProcess::ExitStatus exitStatus) {
        if (proc != proc_) {
            return;
        }
        const bool wasRunning = running_;
        const bool userStopped = stopping_;
        const QString errorText = QString::fromLocal8Bit(stderrBuf_.trimmed());
        running_ = false;
        stopping_ = false;
        proc_ = nullptr;
        proc->deleteLater();
        if (wasRunning) {
            emit runningChanged(false);
        }
        if (!userStopped
            && (exitStatus == QProcess::CrashExit || exitCode != 0)) {
            emit errorOccurred(errorText.isEmpty()
                                   ? QStringLiteral("HTTP 服务已退出（退出码 %1）").arg(exitCode)
                                   : errorText);
        }
    });

    proc->start(program, args);
    return true;
}

void HttpServerController::stop()
{
    if (!proc_) {
        return;
    }
    stopping_ = true;
    proc_->kill();
}

bool HttpServerController::isRunning() const
{
    return running_;
}

int HttpServerController::port() const
{
    return params_.port;
}

QString HttpServerController::bind() const
{
    return params_.bind;
}

QString HttpServerController::lastError() const
{
    return lastError_;
}

QStringList HttpServerController::localIpv4Addresses() const
{
    QStringList result;
    const QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
    for (const QHostAddress& address : addresses) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback()) {
            result << address.toString();
        }
    }
    return result;
}
