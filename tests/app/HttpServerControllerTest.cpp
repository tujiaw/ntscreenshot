#include "app/HttpServerController.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTimer>
#include <QDebug>
#include <QStringList>

namespace {

bool contains(const QStringList& args, const QString& token)
{
    return args.contains(token);
}

bool containsPair(const QStringList& args, const QString& key, const QString& value)
{
    const int index = args.indexOf(key);
    return index >= 0 && index + 1 < args.size() && args.at(index + 1) == value;
}

HttpServerController::Params makeParams()
{
    HttpServerController::Params params;
    params.directory = QStringLiteral("D:/share");
    params.port = 8231;
    params.bind = QStringLiteral("127.0.0.1");
    params.protocol = QStringLiteral("HTTP/1.1");
    params.cgi = false;
    return params;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const HttpServerController::Params params = makeParams();

    // The port must be positional. `-p` is not the port flag, and from Python 3.12
    // on it aliases --protocol, which silently leaves the port unset so the server
    // binds the default 8000 instead of the configured port.
    const QStringList withProtocol =
        HttpServerController::buildServerArguments(params, /*protocolSupported=*/true);
    if (contains(withProtocol, QStringLiteral("-p"))) {
        qCritical() << "port must not be passed as -p:" << withProtocol;
        return 1;
    }
    if (withProtocol.last() != QStringLiteral("8231")) {
        qCritical() << "port must be the trailing positional argument:" << withProtocol;
        return 1;
    }
    if (!containsPair(withProtocol, QStringLiteral("-d"), params.directory)
        || !containsPair(withProtocol, QStringLiteral("-b"), params.bind)) {
        qCritical() << "directory and bind must be passed through:" << withProtocol;
        return 1;
    }
    if (!containsPair(withProtocol, QStringLiteral("--protocol"), QStringLiteral("HTTP/1.1"))) {
        qCritical() << "a capable interpreter must receive --protocol:" << withProtocol;
        return 1;
    }

    // An interpreter below 3.12 rejects --protocol, so the flag must be dropped
    // rather than failing the whole start().
    const QStringList withoutProtocol =
        HttpServerController::buildServerArguments(params, /*protocolSupported=*/false);
    if (contains(withoutProtocol, QStringLiteral("--protocol"))) {
        qCritical() << "an old interpreter must not receive --protocol:" << withoutProtocol;
        return 1;
    }
    if (withoutProtocol.last() != QStringLiteral("8231")) {
        qCritical() << "dropping --protocol must not disturb the port:" << withoutProtocol;
        return 1;
    }

    // An empty protocol never produces a dangling flag.
    HttpServerController::Params blank = params;
    blank.protocol.clear();
    const QStringList blankArgs =
        HttpServerController::buildServerArguments(blank, /*protocolSupported=*/true);
    if (contains(blankArgs, QStringLiteral("--protocol"))) {
        qCritical() << "an empty protocol must not emit --protocol:" << blankArgs;
        return 1;
    }

    // CGI is forwarded, and the port still terminates the argument list.
    HttpServerController::Params cgi = params;
    cgi.cgi = true;
    const QStringList cgiArgs =
        HttpServerController::buildServerArguments(cgi, /*protocolSupported=*/true);
    if (!contains(cgiArgs, QStringLiteral("--cgi")) || cgiArgs.last() != QStringLiteral("8231")) {
        qCritical() << "cgi must be forwarded with the port last:" << cgiArgs;
        return 1;
    }

    // Browsers refuse to open Chromium's restricted ports, so a server bound to
    // one of them answers curl but never a browser. The UI warns on exactly this
    // set, which is why port 22 must be reported and 8000 must not.
    const int blocked[] = {22, 23, 25, 110, 143, 6000, 10080};
    for (int port : blocked) {
        if (!HttpServerController::isBrowserBlockedPort(port)) {
            qCritical() << "port" << port << "must be reported as browser blocked";
            return 1;
        }
    }
    const int reachable[] = {8000, 8080, 9000, 65535};
    for (int port : reachable) {
        if (HttpServerController::isBrowserBlockedPort(port)) {
            qCritical() << "port" << port << "must stay usable from a browser";
            return 1;
        }
    }
    // Out-of-range input is rejected before it can index the table.
    const int outOfRange[] = {0, -1, 65536, 100000};
    for (int port : outOfRange) {
        if (HttpServerController::isBrowserBlockedPort(port)) {
            qCritical() << "port" << port << "is out of range and cannot be blocked";
            return 1;
        }
    }

    if (withProtocol.first() != QStringLiteral("-u")) {
        qCritical() << "readiness banner requires unbuffered stdout";
        return 1;
    }

    if (app.arguments().contains(QStringLiteral("--integration"))) {
        HttpServerController server;
        if (!server.pythonAvailable()) {
            qWarning() << "Python unavailable; skipping integration checks";
            return 77;
        }
        QTemporaryDir directory;
        if (!directory.isValid()) return 1;
        HttpServerController::Params live;
        live.directory = directory.path();
        live.bind = QStringLiteral("127.0.0.1");
        QTcpServer occupied;
        if (!occupied.listen(QHostAddress::LocalHost, 0)) return 1;
        live.port = occupied.serverPort();
        int startedSignals = 0;
        QObject::connect(&server, &HttpServerController::runningChanged, &app,
                         [&](bool running) { if (running) ++startedSignals; });
        if (server.start(live) || server.isRunning() || startedSignals != 0
            || server.lastError().isEmpty()) {
            qCritical() << "occupied port must fail without reporting running";
            return 1;
        }
        occupied.close();
        // Reuse the same controller after failure; Python resolves the hostname.
        live.bind = QStringLiteral("localhost");
        if (!server.start(live) || !server.isRunning() || startedSignals != 1) {
            qCritical() << "hostname startup failed:" << server.lastError();
            return 1;
        }
        QEventLoop stopped;
        QObject::connect(&server, &HttpServerController::runningChanged, &stopped,
                         [&](bool running) { if (!running) stopped.quit(); });
        QTimer::singleShot(5000, &stopped, &QEventLoop::quit);
        server.stop();
        if (server.isRunning()) stopped.exec();
        if (server.isRunning()) return 1;
    }

    qInfo() << "HTTP server argument contract passed";
    return 0;
}
