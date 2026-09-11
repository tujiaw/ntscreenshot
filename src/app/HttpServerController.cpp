#include "HttpServerController.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QProcessEnvironment>

#include <algorithm>
#include <iterator>

namespace {

constexpr int kProbeTimeoutMs = 3000;

// Spawning python is quick; binding the port happens right after. Both are
// bounded so a click can never leave the caller waiting without a verdict.
constexpr int kStartTimeoutMs = 3000;
constexpr int kListenProbeMs = 2000;

// Chromium's restricted ports (net/base/port_util.cc, `kRestrictedPorts`) as of
// that file's published list; Firefox blocks the same family. Browsers refuse to
// open these outright, so a server on one of them only answers curl and other
// non-browser clients. Port 0 is left out because the UI cannot produce it.
// Kept sorted for binary_search.
constexpr int kBrowserBlockedPorts[] = {
    1, 7, 9, 11, 13, 15, 17, 19, 20, 21, 22, 23, 25, 37, 42, 43, 53, 69, 77, 79,
    87, 95, 101, 102, 103, 104, 109, 110, 111, 113, 115, 117, 119, 123, 135, 137,
    139, 143, 161, 179, 389, 427, 465, 512, 513, 514, 515, 526, 530, 531, 532,
    540, 548, 554, 556, 563, 587, 601, 636, 989, 990, 993, 995, 1719, 1720, 1723,
    2049, 3659, 4045, 5060, 5061, 6000, 6566, 6665, 6666, 6667, 6668, 6669, 6697,
    10080,
};

// `--protocol` only exists from Python 3.12 on; earlier interpreters reject the
// flag outright, taking the whole start() down with them. 3.11 is gated off too
// because it was not verified to accept the flag.
constexpr int kProtocolMinMajor = 3;
constexpr int kProtocolMinMinor = 12;

struct PythonInterpreter {
    QString program;
    int major = 0;
    int minor = 0;

    bool isValid() const { return !program.isEmpty(); }

    bool supportsProtocol() const
    {
        return major > kProtocolMinMajor
            || (major == kProtocolMinMajor && minor >= kProtocolMinMinor);
    }
};

PythonInterpreter probePythonInterpreter(const QString& candidate)
{
    PythonInterpreter interpreter;
    const QString executable = QStandardPaths::findExecutable(candidate);
    if (executable.isEmpty()) {
        return interpreter;
    }

    QProcess process;
    process.start(executable, QStringList{QStringLiteral("--version")});
    if (!process.waitForStarted(kProbeTimeoutMs)) {
        return interpreter;
    }
    if (!process.waitForFinished(kProbeTimeoutMs)) {
        process.kill();
        process.waitForFinished();
        return interpreter;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return interpreter;
    }

    // Python before 3.4 prints --version to stderr, so both streams are read.
    const QString versionText = QString::fromUtf8(process.readAllStandardOutput())
                                    + QString::fromUtf8(process.readAllStandardError());
    if (!versionText.contains(QStringLiteral("Python"), Qt::CaseInsensitive)) {
        return interpreter;
    }

    static const QRegularExpression versionPattern(QStringLiteral(R"(Python\s+(\d+)\.(\d+))"));
    const QRegularExpressionMatch match = versionPattern.match(versionText);
    if (match.hasMatch()) {
        interpreter.major = match.captured(1).toInt();
        interpreter.minor = match.captured(2).toInt();
    }
    interpreter.program = executable;
    return interpreter;
}

const PythonInterpreter& cachedPythonInterpreter()
{
    static const PythonInterpreter interpreter = []() {
        PythonInterpreter found = probePythonInterpreter(QStringLiteral("python3"));
        if (!found.isValid()) {
            found = probePythonInterpreter(QStringLiteral("python"));
        }
        return found;
    }();
    return interpreter;
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
    return cachedPythonInterpreter().program;
}

bool HttpServerController::pythonSupportsProtocol() const
{
    return cachedPythonInterpreter().supportsProtocol();
}

QStringList HttpServerController::buildServerArguments(const Params& params, bool protocolSupported)
{
    QStringList args{QStringLiteral("-u"), QStringLiteral("-m"), QStringLiteral("http.server"),
                     QStringLiteral("-d"), params.directory,
                     QStringLiteral("-b"), params.bind};
    if (params.cgi) {
        args << QStringLiteral("--cgi");
    }
    if (protocolSupported && !params.protocol.isEmpty()) {
        args << QStringLiteral("--protocol") << params.protocol;
    }
    // The port is positional and stays last: `-p` is not the port flag, and from
    // Python 3.12 on it is an alias for --protocol, which would silently leave the
    // port unset and make the server listen on the default 8000 instead.
    args << QString::number(params.port);
    return args;
}

bool HttpServerController::isBrowserBlockedPort(int port)
{
    if (port < 1 || port > 65535) {
        return false;
    }
    return std::binary_search(std::begin(kBrowserBlockedPorts),
                              std::end(kBrowserBlockedPorts), port);
}

bool HttpServerController::start(const Params& params)
{
    if (running_) {
        return true;
    }
    if (starting_) return false;
    if (proc_) {
        // A stale process object from a previous run should not exist here, but
        // make sure we are not leaking one.
        proc_->deleteLater();
        proc_ = nullptr;
    }

    lastError_.clear();
    const QString program = pythonProgram();
    if (program.isEmpty()) {
        lastError_ = QStringLiteral("未检测到可用的 Python 解释器，请先安装 Python 3 并加入 PATH。");
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
    starting_ = true;
    startFailure_.clear();
    stderrBuf_.clear();
    stdoutBuf_.clear();

    // An interpreter too old for --protocol still serves fine, just with its own
    // default HTTP version, so the flag is dropped instead of failing the start.
    const QStringList args = buildServerArguments(params_, pythonSupportsProtocol());

    auto* proc = new QProcess(this);
    proc_ = proc;
    proc->setWorkingDirectory(params_.directory);
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("PYTHONIOENCODING"), QStringLiteral("utf-8"));
    proc->setProcessEnvironment(environment);
    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc]() {
        if (proc != proc_) return;
        const QByteArray output = proc->readAllStandardOutput();
        if (starting_) stdoutBuf_.append(output);
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
            const QString message = QStringLiteral("无法启动 Python：%1").arg(proc->errorString());
            running_ = false;
            proc_ = nullptr;
            proc->deleteLater();
            if (starting_) {
                // start() is about to report this through lastError_.
                startFailure_ = message;
                return;
            }
            emit errorOccurred(message);
        }
    });
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int exitCode, QProcess::ExitStatus exitStatus) {
        if (proc != proc_) {
            return;
        }
        const bool wasStarting = starting_;
        const bool wasRunning = running_;
        const bool userStopped = stopping_;
        const QString errorText = QString::fromUtf8(stderrBuf_).trimmed();
        running_ = false;
        stopping_ = false;
        proc_ = nullptr;
        proc->deleteLater();
        if (wasRunning) {
            emit runningChanged(false);
        }
        if (wasStarting) {
            // start() is still waiting on this process and turns the exit into
            // lastError_; raising errorOccurred here as well would double up the
            // dialog for a single failed start.
            startFailure_ = errorText.isEmpty()
                ? QStringLiteral("HTTP 服务已退出（退出码 %1）").arg(exitCode)
                : errorText;
            return;
        }
        if (!userStopped
            && (exitStatus == QProcess::CrashExit || exitCode != 0)) {
            emit errorOccurred(errorText.isEmpty()
                                   ? QStringLiteral("HTTP 服务已退出（退出码 %1）").arg(exitCode)
                                   : errorText);
        }
    });

    proc->start(program, args);

    if (!proc->waitForStarted(kStartTimeoutMs)) {
        starting_ = false;
        if (proc_ == proc) {
            proc_ = nullptr;
            proc->deleteLater();
        }
        lastError_ = startFailure_.isEmpty()
            ? QStringLiteral("无法启动 Python：%1").arg(proc->errorString())
            : startFailure_;
        startFailure_.clear();
        return false;
    }

    // http.server prints this banner only after constructing its server (bind
    // and listen have succeeded). Read it from this child's unbuffered stdout,
    // so another process on the same port can never satisfy readiness.
    bool listening = false;
    QElapsedTimer elapsed;
    elapsed.start();
    while (proc_ == proc && elapsed.elapsed() < kListenProbeMs) {
        proc->waitForReadyRead(qMax(1, kListenProbeMs - static_cast<int>(elapsed.elapsed())));
        if (proc_ != proc) break;
        const auto lines = stdoutBuf_.split('\n');
        for (const QByteArray& line : lines) {
            if (line.startsWith("Serving HTTP on ") && line.contains(" port ")
                && line.trimmed().endsWith(" ...")) {
                listening = proc->state() == QProcess::Running;
                break;
            }
        }
        if (listening) break;
    }
    stdoutBuf_.clear();

    if (listening) {
        starting_ = false;
        running_ = true;
        emit runningChanged(true);
        return true;
    }

    // Not serving. Whatever the process is doing, it must not be left behind
    // claiming to serve, and the wait above has to end in a reported failure.
    bool killedByUs = false;
    if (proc_ == proc) {
        // Still alive but never accepted a connection: this exit is our doing.
        killedByUs = proc->state() == QProcess::Running;
        stopping_ = true;
        proc->kill();
        proc->waitForFinished(kStartTimeoutMs);
        stopping_ = false;
        if (proc_ == proc) {
            proc_ = nullptr;
            proc->deleteLater();
        }
    }
    starting_ = false;

    // A process we killed ourselves says nothing about why the port never came
    // up, so its exit code must not mask the explanation below.
    lastError_ = killedByUs ? QString() : startFailure_;
    startFailure_.clear();
    if (lastError_.isEmpty()) {
        lastError_ = QStringLiteral("端口 %1 未能在 %2 秒内开始监听，可能已被其他程序占用或被防火墙拦截。")
                         .arg(params_.port)
                         .arg(kListenProbeMs / 1000.0, 0, 'g', 2);
    }
    return false;
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
