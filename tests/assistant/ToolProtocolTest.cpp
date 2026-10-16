#include "modules/assistant/runtime/agent/OpenAI.h"
#include "core/settings/SettingModel.h"
#include <QApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QEventLoop>
#include <QTimer>
#include <cstdio>

// Isolate settings from the user's configuration; all protocol/network code is real.
static QString configPath;
namespace Util { QString getConfigPath() { return configPath; } }

static QByteArray response(const QString &text, const QJsonArray &calls = {}) {
    QJsonObject message{{"role", "assistant"}, {"content", text}};
    if (!calls.isEmpty()) message.insert("tool_calls", calls);
    return QJsonDocument(QJsonObject{{"choices", QJsonArray{QJsonObject{
        {"message", message}, {"finish_reason", calls.isEmpty() ? "stop" : "tool_calls"}}}}}).toJson(QJsonDocument::Compact);
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QTemporaryDir directory;
    configPath = directory.filePath("settings.ini");
    QSettings initial(configPath, QSettings::IniFormat);
    initial.setValue("test", true); initial.sync();
    SettingModel settings(nullptr);
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost)) return 1;
    settings.setLlmProviders({{QStringLiteral("test"), QStringLiteral("http://127.0.0.1:%1/v1").arg(server.serverPort()),
                              QStringLiteral("test"), QStringLiteral("test"), 0.0}});
    settings.setLlmActiveProviderIndex(0);
    QList<QByteArray> responses;
    QList<QJsonObject> requests;
    QObject::connect(&server, &QTcpServer::newConnection, &app, [&]() {
        auto *socket = server.nextPendingConnection();
        auto buffer = QSharedPointer<QByteArray>::create();
        QObject::connect(socket, &QTcpSocket::readyRead, socket, [&, socket, buffer]() {
            buffer->append(socket->readAll());
            int boundary = buffer->indexOf("\r\n\r\n");
            if (boundary < 0) return;
            int size = 0;
            for (const auto &line : buffer->left(boundary).split('\n'))
                if (line.toLower().startsWith("content-length:")) size = line.mid(15).trimmed().toInt();
            if (buffer->size() < boundary + 4 + size) return;
            requests.append(QJsonDocument::fromJson(buffer->mid(boundary + 4, size)).object());
            const QByteArray body = responses.isEmpty() ? response(QStringLiteral("unexpected")) : responses.takeFirst();
            socket->write("HTTP/1.1 200 OK\r\nContent-Type: " + QByteArray(body.startsWith("data:") ? "text/event-stream" : "application/json")
                          + "\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
            socket->disconnectFromHost();
        });
        QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    });
    const QJsonArray definitions = QJsonDocument::fromJson(R"([{"type":"function","function":{"name":"read","parameters":{"type":"object","required":["url"],"properties":{"url":{"type":"string"}}}}}])").array();
    const QJsonArray goodCalls = QJsonDocument::fromJson(R"([{"id":"call_1","type":"function","function":{"name":"read","arguments":"{\"url\":\"https://example.com\"}"}}])").array();
    const QJsonArray badCalls = QJsonDocument::fromJson(R"([{"id":"call_1","type":"function","function":{"name":"read","arguments":"{}"}}])").array();
    const QString leak = QStringLiteral("<｜｜DSML｜｜ calls><｜｜DSML｜｜ invoke name=\"read\">");
    QByteArray sse;
    for (const QChar ch : leak) {
        sse += "data: " + QJsonDocument(QJsonObject{{"choices", QJsonArray{QJsonObject{{"delta", QJsonObject{{"content", QString(ch)}}}}}}}).toJson(QJsonDocument::Compact) + "\n\n";
    }
    sse += "data: [DONE]\n\n";
    const QByteArray toolStream =
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_1\",\"type\":\"function\",\"function\":{\"name\":\"read\",\"arguments\":\"{\\\"url\\\":\"}}]}}]}\n\n"
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"\\\"https://example.com\\\"}\"}}]}}]}\n\n";
    const QByteArray toolStreamEnd = "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"tool_calls\"}]}\n\ndata: [DONE]\n\n";
    QJsonObject secondBadCall = badCalls.first().toObject();
    secondBadCall.insert("id", "call_2");

    OpenAIChat chat(&settings);
    for (int scenario = 0; scenario < 7; ++scenario) {
        requests.clear();
        responses = scenario == 0 ? QList<QByteArray>{response(leak), response("", goodCalls)}
            : scenario == 1 ? QList<QByteArray>{response("", badCalls), response("", badCalls), response("", badCalls)}
            : scenario == 2 ? QList<QByteArray>{sse, response(QStringLiteral("已完成"))}
            : scenario == 3 ? QList<QByteArray>{response("", QJsonArray{goodCalls.first(), secondBadCall}), response("", goodCalls)}
            : scenario == 4 ? QList<QByteArray>{response(leak)}
            : scenario == 5 ? QList<QByteArray>{toolStream + toolStreamEnd}
            : QList<QByteArray>{toolStream, response("", goodCalls)};
        chat.setToolDefinitions(definitions);
        chat.setStreamingEnabled(scenario == 2 || scenario >= 5);
        int executedBatches = 0, errors = 0;
        QString shown;
        bool timedOut = false;
        QEventLoop loop;
        QTimer timeout; timeout.setSingleShot(true);
        QObject::connect(&timeout, &QTimer::timeout, &loop, [&]() { timedOut = true; loop.quit(); });
        QObject::connect(&chat, &OpenAIChat::sigToolCallsReceived, &loop, [&](const QJsonArray &calls) {
            if (calls == goodCalls) ++executedBatches;
            loop.quit();
        });
        QObject::connect(&chat, &OpenAIChat::sigError, &loop, [&](const QString &) { ++errors; loop.quit(); });
        QObject::connect(&chat, &OpenAIChat::sigStreamDelta, &loop, [&](const QString &text) { shown += text; });
        QObject::connect(&chat, &OpenAIChat::sigStreamFinished, &loop, [&](const QString &text) { shown += text; loop.quit(); });
        QObject::connect(&chat, &OpenAIChat::sigResponse, &loop, [&](const QString &text) { shown += text; loop.quit(); });
        if (scenario == 4) QObject::connect(&chat, &OpenAIChat::sigStreamDiscarded, &loop, [&]() {
            QTimer::singleShot(0, &chat, [&]() { chat.abortActiveReply(); });
            QTimer::singleShot(50, &loop, &QEventLoop::quit);
        });
        timeout.start(5000);
        chat.sendMessage(QStringLiteral("read the page"));
        loop.exec();
        for (const auto &message : chat.conversationMessages()) {
            if (message.toObject().value("role") == QStringLiteral("system")) {
                std::fprintf(stderr, "temporary correction instructions must not persist in conversation history\n");
                return 1;
            }
        }
        const int expectedRequests = scenario == 1 ? 3 : (scenario == 4 || scenario == 5) ? 1 : 2;
        if (timedOut || requests.size() != expectedRequests || shown.contains(QStringLiteral("DSML"))
            || errors != (scenario == 1 ? 1 : 0)
            || executedBatches != ((scenario == 0 || scenario == 3 || scenario >= 5) ? 1 : 0)
            || (scenario == 2 && shown != QStringLiteral("已完成"))) {
            std::fprintf(stderr, "protocol scenario %d failed: requests=%lld errors=%d batches=%d timeout=%d\n",
                         scenario, static_cast<long long>(requests.size()), errors, executedBatches, timedOut);
            return 1;
        }
        if (scenario != 4 && scenario != 5) {
            const auto messages = requests.at(1).value("messages").toArray();
            const auto feedback = QJsonDocument::fromJson(messages.last().toObject().value("content").toString().toUtf8()).object();
            if (!feedback.value("retryable").toBool() || feedback.value("executed").toBool(true)) return 1;
        }
    }
    return 0;
}
