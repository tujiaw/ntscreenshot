#include "modules/assistant/ui/BrowserPanel.h"
#include "modules/assistant/runtime/tools/ToolAbort.h"
#include <QApplication>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QEventLoop>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonArray>
#include <QThread>
#include <QTimer>
#include <iostream>

static void check(bool value, const QString &message)
{
    if (!value) { std::cerr << message.toStdString() << '\n'; std::exit(1); }
}

int main(int argc, char **argv)
{
    QApplication app(argc,argv);
    QTimer watchdog;
    QObject::connect(&watchdog,&QTimer::timeout,&app,[]{ std::cerr << "Test timeout\n"; std::exit(2); });
    watchdog.start(90000);
    QTcpServer server;
    check(server.listen(QHostAddress::LocalHost),"server listen");
    QObject::connect(&server,&QTcpServer::newConnection,&app,[&]{
        while (auto *socket = server.nextPendingConnection()) {
            QObject::connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
            QObject::connect(socket,&QTcpSocket::readyRead,socket,[socket]{
                const auto request = socket->readAll();
                if (request.contains("/hang")) return;
                QByteArray body;
                if (request.contains("GET /login "))
                    body = "<html><body>LOGIN_SECRET<input type=password value=NEVER_EXPOSE><button onclick=\"document.cookie='session=ok; path=/';location.href='/account'\">Sign in</button></body></html>";
                else if (request.contains("GET /account "))
                    body = request.contains("session=ok") ? "<html><body>ACCOUNT_READY<a href='/'>Home</a></body></html>" : "<html><body>NO_SESSION</body></html>";
                else
                    body = "<html><title>Browser use test</title><body><h1>Browser workspace</h1><input id=name placeholder=Name>"
                        "<button onclick=\"document.getElementById('result').innerText='Hello '+document.getElementById('name').value\">Apply</button>"
                        "<select><option value=a>Alpha</option><option value=b>Beta</option></select><p id=result>Ready</p>"
                        "<a href='/login'>Login</a><script>setTimeout(()=>document.getElementById('result').innerText='DYNAMIC_READY',100)</script></body></html>";
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
        }
    });
    const QString base = QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort());
    auto *panel = new BrowserPanel;
    panel->resize(850,600);
    panel->show();
    auto run = [&](const QJsonObject &args, bool cancel = false) {
        LlmTools::ToolAbort abort;
        QString output;
        QEventLoop loop;
        auto *worker = QThread::create([&]{ output = panel->execute(args,&abort); });
        QObject::connect(worker,&QThread::finished,&loop,&QEventLoop::quit);
        if (cancel) QTimer::singleShot(350,&loop,[&]{abort.abort();});
        worker->start(); loop.exec(); worker->wait(); delete worker;
        return output;
    };
    auto read = [&](const QJsonObject &args) { return QJsonDocument::fromJson(run(args).toUtf8()).object(); };
    auto snapshot = read({{"action","open"},{"url",base}});
    check(snapshot.value("content").toString().contains("DYNAMIC_READY"),QString::fromUtf8(QJsonDocument(snapshot).toJson()));
    check(snapshot.value("elements").toArray().size() == 4,"elements extracted");
    snapshot = read({{"action","fill"},{"snapshot",snapshot.value("snapshot")},{"element",1},{"text","Ada"}});
    const QString oldSnapshot = snapshot.value("snapshot").toString();
    snapshot = read({{"action","click"},{"snapshot",oldSnapshot},{"element",2}});
    check(snapshot.value("content").toString().contains("Hello Ada"),"fill and click did not update page");
    check(read({{"action","click"},{"snapshot",oldSnapshot},{"element",2}}).contains("error"),"stale snapshot accepted");
    snapshot = read({{"action","select"},{"snapshot",snapshot.value("snapshot")},{"element",3},{"text","b"}});
    check(snapshot.contains("snapshot"),"select failed");
    auto *view = panel->findChild<QWebEngineView *>();
    check(view != nullptr,"visible web view missing");
    bool selected = false;
    QEventLoop selectedLoop;
    view->page()->runJavaScript("document.querySelector('select').value",[&](const QVariant &v){ selected = v.toString() == "b"; selectedLoop.quit(); });
    selectedLoop.exec(); check(selected,"select value not applied");

    // 认证页应自动暂停并把控制权交给用户，用户完成登录后无需任何按钮，AI 自动继续。
    int attention = 0;
    const auto connection = QObject::connect(panel,&BrowserPanel::attentionRequired,panel,[&](const QString &){
        ++attention;
        // Simulate the human completing authentication in exactly the same visible page.
        QTimer::singleShot(250,panel,[&]{ view->page()->runJavaScript("document.querySelector('button').click()"); });
    });
    const QString loggedIn = run({{"action","open"},{"url",base+"/login"}});
    check(attention == 1,"login did not pause for user");
    check(loggedIn.contains("ACCOUNT_READY") && !loggedIn.contains("LOGIN_SECRET") && !loggedIn.contains("NEVER_EXPOSE"),"login handoff did not preserve session or leaked authentication page");
    QObject::disconnect(connection);
    check(run({{"action","open"},{"url",base+"/account"}}).contains("ACCOUNT_READY"),"cookies not retained across calls");
    check(run({{"action","open"},{"url","file:///C:/Windows/win.ini"}}).contains("HTTP/HTTPS"),"unsafe URL accepted");
    check(run({{"action","wait_user"}},true).contains(QStringLiteral("取消")),"cancel while waiting failed");
    panel->resume();
    check(run({{"action","open"},{"url",base+"/hang"}},true).contains(QStringLiteral("取消")),"cancel navigation failed");
    snapshot = read({{"action","open"},{"url",base}});
    check(snapshot.contains("snapshot"),"cannot recover after cancellation");
    if (app.arguments().contains("--capture")) panel->grab().save("browser-panel.png");
    // Destruction wakes a waiting worker and destroys page before profile.
    QTimer::singleShot(350,&app,[&]{ delete panel; panel = nullptr; });
    check(run({{"action","wait_user"}}).contains(QStringLiteral("关闭")),"close did not wake waiting worker");
    panel = new BrowserPanel;
    check(run({{"action","open"},{"url",base+"/account"}}).contains("NO_SESSION"),"cookies leaked into another conversation");
    delete panel;
    // Flush explicitly: teardown of the WebEngine process aborts on some machines, which
    // would otherwise discard the buffered result line.
    std::cout << "Browser use: navigation, DOM actions, stale handles, login handoff, auto resume, credential exclusion, session isolation, cancellation, close passed" << std::endl;
    return 0;
}
