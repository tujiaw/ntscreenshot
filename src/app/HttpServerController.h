#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class QProcess;

// Long-lived (app-level) controller for a background `python -m http.server`
// child process. Owned by WindowManager so the server keeps running while the
// Settings dialog is closed and is torn down when the app exits.
class HttpServerController : public QObject {
    Q_OBJECT

public:
    struct Params {
        QString directory;
        int port = 8000;
        QString bind = QStringLiteral("0.0.0.0");
        QString protocol = QStringLiteral("HTTP/1.1");
        bool cgi = false;
    };

    explicit HttpServerController(QObject* parent = nullptr);
    ~HttpServerController() override;

    // Self-contained python discovery (python3 -> python fallback), cached.
    bool pythonAvailable() const;
    QString pythonProgram() const;

    // Whether the detected interpreter accepts `--protocol` (Python >= 3.12).
    bool pythonSupportsProtocol() const;

    // Arguments for `python -m http.server`. Pure and static so tests can cover
    // it without spawning a process.
    static QStringList buildServerArguments(const Params& params, bool protocolSupported);

    // Synchronous validation failures are reported through lastError(); returns
    // false without touching a running server when already running. Runtime
    // problems (e.g. port already bound) surface later via errorOccurred().
    bool start(const Params& params);
    void stop();

    bool isRunning() const;
    int port() const;
    QString bind() const;
    QString lastError() const;

    // IPv4 addresses (excluding loopback) reachable when bound to 0.0.0.0.
    QStringList localIpv4Addresses() const;

signals:
    void runningChanged(bool running);
    void errorOccurred(const QString& message);

private:
    void resetState();

    QProcess* proc_ = nullptr;
    Params params_;
    bool running_ = false;
    bool stopping_ = false;
    QByteArray stderrBuf_;
    QString lastError_;
};
