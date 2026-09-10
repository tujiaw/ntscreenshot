#include "app/HttpServerController.h"

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

int main()
{
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

    qInfo() << "HTTP server argument contract passed";
    return 0;
}
