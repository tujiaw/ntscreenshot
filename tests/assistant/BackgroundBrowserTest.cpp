#include "modules/assistant/runtime/tools/BackgroundBrowser.h"
#include "modules/assistant/runtime/tools/ToolAbort.h"

#include <QApplication>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>
#include <iostream>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost)) return 1;
    QObject::connect(&server, &QTcpServer::newConnection, &app, [&] {
        while (auto *socket = server.nextPendingConnection()) {
            QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket] {
                const auto request = socket->readAll();
                if (request.contains("/hang")) return;
                const QByteArray body = "<html><title>Browser test</title><body>Initial"
                    "<script>setTimeout(()=>{document.body.innerHTML='DYNAMIC_CONTENT"
                    "<a href=\"https://example.com/source\">Source</a>'},200)</script></body></html>";
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: "
                              + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
        }
    });
    const QString base = QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort());
    LlmTools::BackgroundBrowser browser;
    auto run = [&](const QUrl &url, bool cancel) {
        LlmTools::ToolAbort abort;
        QString output;
        QEventLoop loop;
        QThread *worker = QThread::create([&] { output = browser.read(url, 20000, &abort); });
        QObject::connect(worker, &QThread::finished, &loop, &QEventLoop::quit);
        if (cancel) QTimer::singleShot(300, &loop, [&] { abort.abort(); });
        worker->start();
        loop.exec();
        worker->wait();
        delete worker;
        // Cancellation is observed by the GUI timer; then DeferredDelete tears down page/profile.
        QEventLoop cleanup;
        QTimer::singleShot(150, &cleanup, &QEventLoop::quit);
        cleanup.exec();
        if (!browser.children().isEmpty()) {
            std::cerr << "Browser request resources leaked\n";
            std::exit(1);
        }
        return output;
    };
    for (int i = 0; i < 3; ++i) {
        const auto output = run(QUrl(base), false);
        const auto doc = QJsonDocument::fromJson(output.toUtf8()).object();
        if (!doc.value("content").toString().contains("DYNAMIC_CONTENT") ||
            !output.contains("https://example.com/source") || doc.value("title") != "Browser test") {
            std::cerr << "Dynamic page extraction failed: " << output.toStdString() << '\n';
            return 1;
        }
    }
    if (!run(QUrl(base + "/hang"), true).contains(QStringLiteral("取消"))) return 1;
    if (!run(QUrl("file:///C:/Windows/win.ini"), false).contains(QStringLiteral("HTTP/HTTPS"))) return 1;
    if (!run(QUrl(base + "/hang"), false).contains(QStringLiteral("超时"))) return 1;
    {
        // Closing the conversation while a request is loading must wake the worker.
        auto *closingBrowser = new LlmTools::BackgroundBrowser;
        QString output;
        QEventLoop loop;
        auto *worker = QThread::create([&] { output = closingBrowser->read(QUrl(base + "/hang"), 20000, nullptr); });
        QObject::connect(worker, &QThread::finished, &loop, &QEventLoop::quit);
        QTimer::singleShot(300, &loop, [&] { delete closingBrowser; });
        worker->start();
        loop.exec();
        worker->wait();
        delete worker;
        if (!output.contains(QStringLiteral("关闭"))) return 1;
    }
    if (app.arguments().contains(QStringLiteral("--live"))) {
        const auto output = run(QUrl(QStringLiteral("https://www.bing.com/search?q=Qt+QWebEnginePage")), false);
        if (QJsonDocument::fromJson(output.toUtf8()).object().value("content").toString().isEmpty()) {
            std::cerr << "Live search failed: " << output.toStdString() << '\n';
            return 1;
        }
        std::cout << output.toStdString() << '\n';
    }
    if (!QApplication::topLevelWidgets().isEmpty()) return 1;
    std::cout << "Background dynamic reading, links, cancellation, timeout, resource cleanup and no-window checks passed\n";
    return 0;
}
