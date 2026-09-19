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
#include <QLabel>
#include <QPushButton>
#include <QKeyEvent>
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
                else if (request.contains("GET /optional "))
                    body = "<html><body><h1>PUBLIC_ARTICLE</h1><form><input name=username value=PRIVATE_USER><input type=password value=NEVER_EXPOSE></form><button onclick=\"document.getElementById('result').innerText='PUBLIC_DONE'\">Public action</button><p id=result>Ready</p></body></html>";
                else if (request.contains("GET /long ")) {
                    body = "<html><body><p id=long>" + QByteArray(14000,'x') + "</p><p>NEEDLE_TARGET</p>";
                    for (int i=0;i<45;++i) body += "<button>Item " + QByteArray::number(i) + "</button>";
                    body += "</body></html>";
                }
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
    auto *panel = new BrowserPanel(nullptr, 8000);
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
    snapshot = read({{"action","click"},{"snapshot",oldSnapshot},{"element",2}});
    check(snapshot.contains("error") && snapshot.contains("snapshot") && !snapshot.value("actionExecuted").toBool(),"stale snapshot must include recovery snapshot without acting");
    snapshot = read({{"action","select"},{"snapshot",snapshot.value("snapshot")},{"element",3},{"text","b"}});
    check(snapshot.contains("snapshot"),"select failed");
    auto *view = panel->findChild<QWebEngineView *>();
    check(view != nullptr,"visible web view missing");
    bool selected = false;
    QEventLoop selectedLoop;
    view->page()->runJavaScript("document.querySelector('select').value",[&](const QVariant &v){ selected = v.toString() == "b"; selectedLoop.quit(); });
    selectedLoop.exec(); check(selected,"select value not applied");
    auto js = [&](const QString &source) {
        QEventLoop loop;
        view->page()->runJavaScript(source,[&](const QVariant &){loop.quit();});
        loop.exec();
    };
    js("document.getElementById('result').textContent='Unrelated update'");
    snapshot = read({{"action","click"},{"snapshot",snapshot.value("snapshot")},{"element",2}});
    check(!snapshot.contains("error"),"unrelated DOM update invalidated target");
    js("document.querySelector('button').textContent='Delete everything'");
    snapshot = read({{"action","click"},{"snapshot",snapshot.value("snapshot")},{"element",2}});
    check(snapshot.value("code") == "target_changed" && snapshot.contains("snapshot"),"changed target meaning was not rejected with recovery snapshot");
    js("document.querySelector('button').outerHTML='<button>Replacement</button>'");
    snapshot = read({{"action","click"},{"snapshot",snapshot.value("snapshot")},{"element",2}});
    check(snapshot.value("code") == "target_unavailable","replaced target was accepted");

    snapshot = read({{"action","open"},{"url",base+"/long"}});
    check(snapshot.value("content").toString().size() <= 2500,"summary exceeds budget");
    snapshot = read({{"action","read"},{"mode","full"}});
    check(snapshot.value("content").toString().size() == 6000 && snapshot.value("nextOffset").toInt() == 6000,"full text pagination failed");
    check(snapshot.value("elements").toArray().size() == 30 && snapshot.value("nextElementOffset").toInt() == 30,"element page limit failed");
    const QString fullContent = snapshot.value("content").toString();
    snapshot = read({{"action","read"},{"mode","full"},{"since",snapshot.value("snapshot")}});
    check(snapshot.value("contentUnchanged").toBool() && !snapshot.contains("content"),"unchanged text was resent");
    js("document.getElementById('long').firstChild.replaceData(10,1,'CHANGED')");
    snapshot = read({{"action","read"},{"mode","full"},{"since",snapshot.value("snapshot")}});
    const QJsonObject patch = snapshot.value("contentPatch").toObject();
    QString patched = fullContent;
    patched.replace(patch.value("start").toInt(),patch.value("deleteCount").toInt(),patch.value("text").toString());
    const auto fresh = read({{"action","read"},{"mode","full"}});
    check(!patch.isEmpty() && patched == fresh.value("content").toString(),"incremental patch cannot reconstruct content");
    snapshot = read({{"action","read"},{"mode","full"},{"offset",6000},{"elementOffset",30}});
    check(snapshot.value("elements").toArray().size() == 15 && snapshot.value("nextElementOffset").isNull(),"remaining elements are inaccessible");
    snapshot = read({{"action","read"},{"mode","focused"},{"query","NEEDLE_TARGET"}});
    check(snapshot.value("content").toString() == "NEEDLE_TARGET","focused read returned unrelated text");
    panel->clearSnapshotCache();
    snapshot = read({{"action","click"},{"snapshot",snapshot.value("snapshot")},{"element",1}});
    check(snapshot.value("code") == "stale_snapshot" && snapshot.contains("snapshot"),"clearing conversation did not invalidate browser cache");

    // Optional login components must not block public content or ordinary actions.
    int attention = 0;
    auto counter = QObject::connect(panel,&BrowserPanel::attentionRequired,panel,[&](const QString &){ ++attention; });
    snapshot = read({{"action","open"},{"url",base+"/optional"}});
    check(attention == 0 && snapshot.value("authenticationUiPresent").toBool() && snapshot.value("content").toString().contains("PUBLIC_ARTICLE"),"optional login blocked public reading");
    const QString optionalResult = QString::fromUtf8(QJsonDocument(snapshot).toJson());
    check(!optionalResult.contains("PRIVATE_USER") && !optionalResult.contains("NEVER_EXPOSE"),"credential values leaked in snapshot");
    snapshot = read({{"action","click"},{"snapshot",snapshot.value("snapshot")},{"element",3}});
    check(attention == 0 && snapshot.value("content").toString().contains("PUBLIC_DONE"),"optional login blocked public action");
    QObject::disconnect(counter);

    // Only an explicit task-related handoff pauses; completing login resumes automatically.
    snapshot = read({{"action","open"},{"url",base+"/login"}});
    const auto connection = QObject::connect(panel,&BrowserPanel::attentionRequired,panel,[&](const QString &){
        ++attention;
        auto *countdown = panel->findChild<QLabel *>(QStringLiteral("browserAuthenticationCountdown"));
        check(countdown && countdown->isVisible() && !countdown->text().isEmpty(),"authentication countdown not shown");
        // Simulate the human completing authentication in exactly the same visible page.
        QTimer::singleShot(250,panel,[&]{ view->page()->runJavaScript("document.querySelector('button').click()"); });
    });
    const QString loggedIn = run({{"action","wait_user"},{"reason","The requested account page requires authentication"}});
    check(attention == 1,"login did not pause for user");
    check(loggedIn.contains("ACCOUNT_READY") && !loggedIn.contains("LOGIN_SECRET") && !loggedIn.contains("NEVER_EXPOSE"),"login handoff did not preserve session or leaked authentication page");
    QObject::disconnect(connection);
    check(run({{"action","open"},{"url",base+"/account"}}).contains("ACCOUNT_READY"),"cookies not retained across calls");
    panel->resetAuthenticationWait();
    snapshot = read({{"action","open"},{"url",base+"/optional"}});
    counter = QObject::connect(panel,&BrowserPanel::attentionRequired,panel,[&](const QString &){ ++attention; });
    QElapsedTimer waitTime;
    waitTime.start();
    QTimer::singleShot(1000,panel,[&]{
        QKeyEvent activity(QEvent::KeyPress,Qt::Key_Shift,Qt::ShiftModifier);
        QApplication::sendEvent(view,&activity);
    });
    snapshot = read({{"action","wait_user"},{"reason","Test an authentication-gated task"}});
    check(snapshot.value("authenticationStatus") == "timed_out" && snapshot.contains("snapshot") && waitTime.elapsed() >= 8800,"idle timeout did not reset on user activity or return current page to AI");
    const int afterTimeout = attention;
    snapshot = read({{"action","wait_user"},{"reason","Repeated authentication request"}});
    check(attention == afterTimeout && snapshot.value("authenticationStatus") == "skipped","timeout caused repeated authentication prompts");
    snapshot = read({{"action","click"},{"snapshot",snapshot.value("snapshot")},{"element",3}});
    check(snapshot.value("content").toString().contains("PUBLIC_DONE"),"cannot continue public task after timeout");
    panel->resetAuthenticationWait();
    const auto skipConnection = QObject::connect(panel,&BrowserPanel::attentionRequired,panel,[&](const QString &){
        QTimer::singleShot(100,panel,[&]{ panel->findChild<QPushButton *>(QStringLiteral("browserSkipAuthentication"))->click(); });
    });
    snapshot = read({{"action","fill"},{"snapshot",snapshot.value("snapshot")},{"element",2},{"text","DO_NOT_WRITE"}});
    check(snapshot.value("authenticationStatus") == "skipped","sensitive field did not hand off or skip failed");
    QObject::disconnect(skipConnection);
    QObject::disconnect(counter);
    check(run({{"action","open"},{"url","file:///C:/Windows/win.ini"}}).contains("HTTP/HTTPS"),"unsafe URL accepted");
    panel->resetAuthenticationWait();
    check(run({{"action","wait_user"},{"reason","Cancellation test"}},true).contains(QStringLiteral("取消")),"cancel while waiting failed");
    check(run({{"action","open"},{"url",base+"/hang"}},true).contains(QStringLiteral("取消")),"cancel navigation failed");
    snapshot = read({{"action","open"},{"url",base}});
    check(snapshot.contains("snapshot"),"cannot recover after cancellation");
    if (app.arguments().contains("--capture")) panel->grab().save("browser-panel.png");
    // Destruction wakes a waiting worker and destroys page before profile.
    QTimer::singleShot(350,&app,[&]{ delete panel; panel = nullptr; });
    check(run({{"action","wait_user"},{"reason","Close test"}}).contains(QStringLiteral("关闭")),"close did not wake waiting worker");
    panel = new BrowserPanel;
    check(run({{"action","open"},{"url",base+"/account"}}).contains("NO_SESSION"),"cookies leaked into another conversation");
    delete panel;
    // Flush explicitly: teardown of the WebEngine process aborts on some machines, which
    // would otherwise discard the buffered result line.
    std::cout << "Browser use: navigation, DOM actions, stale handles, login handoff, auto resume, credential exclusion, session isolation, cancellation, close passed" << std::endl;
    return 0;
}
