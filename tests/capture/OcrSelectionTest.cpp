#include "core/imaging/OcrResult.h"
#include "modules/capture/annotation/OcrSelectionOverlay.h"

#include <QApplication>
#include <QClipboard>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <cstdio>
#include <functional>

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "Failed line %d: %s\n", __LINE__, #condition); return 1; } } while (false)

const QByteArray fixture = R"({"result":{"rec_texts":["hello world","你好"],
    "rec_polys":[[[10,10],[70,10],[70,20],[10,20]],[[10,30],[40,30],[40,40],[10,40]]],
    "text_word":[["hello","world"],["你","好"]],
    "text_word_boxes":[[[10,10,30,20],[40,10,70,20]],[[10,30,25,40],[25,30,40,40]]]}})";

void mouse(QWidget& widget, QEvent::Type type, QPointF point, Qt::MouseButton button, Qt::MouseButtons buttons)
{
    QMouseEvent event(type, point, widget.mapToGlobal(point.toPoint()), button, buttons, Qt::NoModifier);
    QApplication::sendEvent(&widget, &event);
}

void key(QWidget& widget, int code, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    QKeyEvent event(QEvent::KeyPress, code, modifiers);
    QApplication::sendEvent(&widget, &event);
}

bool waitFor(const std::function<bool()>& condition)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < 8000) {
        QApplication::processEvents();
        QThread::msleep(5);
    }
    return condition();
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    const auto result = OcrResult::parse(fixture);
    CHECK(result.lines.size() == 2 && result.spans.size() == 4);
    CHECK(result.selectedText(0, 1) == QStringLiteral("hello world"));
    CHECK(result.selectedText(2, 0) == QStringLiteral("hello world\n你"));
    CHECK(result.selectedText(-1, 0).isEmpty());
    // OCR may return separate blocks on the same visual row, in arbitrary order.
    const auto splitRows = OcrResult::parse(R"({"rec_texts":["right","next","left"],
        "rec_boxes":[[80,11,120,21],[10,30,50,40],[10,10,50,20]]})");
    CHECK(splitRows.selectedText(0, 2) == QStringLiteral("left right\nnext"));
    CHECK(splitRows.selectedText(2, 0) == QStringLiteral("left right\nnext"));
    CHECK(splitRows.selectedText(0, 0) == QStringLiteral("right"));
    const auto touchingRows = OcrResult::parse(R"({"rec_texts":["upper","lower"],
        "rec_boxes":[[10,10,50,20],[70,19,110,29]]})");
    CHECK(touchingRows.selectedText(0, 1) == QStringLiteral("upper\nlower"));
    const auto splitWords = OcrResult::parse(R"({"rec_texts":["hello  world","你好"],
        "rec_boxes":[[10,10,70,20],[90,8,120,22]],
        "text_word":[["hello","world"],["你","好"]],
        "text_word_boxes":[[[10,10,30,20],[40,10,70,20]],[[90,8,105,22],[105,8,120,22]]]})");
    CHECK(splitWords.selectedText(0, 3) == QStringLiteral("hello  world 你好"));
    CHECK(splitWords.selectedText(1, 2) == QStringLiteral("world 你"));
    CHECK(splitWords.selectedText(2, 1) == QStringLiteral("world 你"));
    CHECK(OcrResult::parse("not json").spans.isEmpty());
    CHECK(OcrResult::parse(QJsonDocument::fromJson(fixture).toJson(QJsonDocument::Compact) + '\n' + QJsonDocument::fromJson(fixture).toJson(QJsonDocument::Compact)).spans.size() == 8);
    CHECK(OcrResult::parse(R"({"rec_texts":["line"],"rec_boxes":[[1,2,30,12]]})").spans.size() == 1);
    CHECK(OcrResult::parse(R"({"rec_texts":["line"],"dt_polys":[[[1,2],[30,2],[30,12],[1,12]]]})").spans.isEmpty());
    CHECK(OcrResult::parse(R"({"rec_texts":["line"],"rec_boxes":[[30,12,1,2]]})").spans.isEmpty());
    CHECK(OcrResult::parse(R"({"rec_texts":["ab"],"rec_boxes":[[0,0,30,10]],"text_word":[["a"]],"text_word_boxes":[[[0,0,10,10]]]})").spans.size() == 1);
    auto object = QJsonDocument::fromJson(fixture).object().value("result").toObject();
    object.insert("text_word_region", object.take("text_word_boxes"));
    CHECK(OcrResult::parse(QJsonDocument(object).toJson()).spans.size() == 4);

    QWidget host;
    host.resize(200, 120);
    OcrSelectionOverlay overlay(&host, true);
    host.show();
    QApplication::processEvents();
    QApplication::clipboard()->setText(QStringLiteral("keep clipboard"));
    overlay.setResult(result, QSize(100, 60));
    CHECK(overlay.isVisible());
    const QImage plainOverlay = overlay.grab().toImage();
    CHECK(QApplication::clipboard()->text() == QStringLiteral("keep clipboard"));
    mouse(overlay, QEvent::MouseMove, {30, 30}, Qt::NoButton, Qt::NoButton);
    CHECK(overlay.cursor().shape() == Qt::IBeamCursor);
    mouse(overlay, QEvent::MouseButtonPress, {30, 30}, Qt::LeftButton, Qt::LeftButton);
    mouse(overlay, QEvent::MouseMove, {110, 30}, Qt::NoButton, Qt::LeftButton);
    mouse(overlay, QEvent::MouseButtonRelease, {110, 30}, Qt::LeftButton, Qt::NoButton);
    CHECK(overlay.selectedText() == QStringLiteral("hello world"));
    const QImage selectedOverlay = overlay.grab().toImage();
    CHECK(selectedOverlay.pixelColor(30, 30) != plainOverlay.pixelColor(30, 30));
    // Continuous text selection also covers spaces and the outer glyph edges.
    CHECK(selectedOverlay.pixelColor(70, 30) != plainOverlay.pixelColor(70, 30));
    CHECK(selectedOverlay.pixelColor(19, 30) != plainOverlay.pixelColor(19, 30));
    CHECK(selectedOverlay.pixelColor(140, 30) != plainOverlay.pixelColor(140, 30));
    CHECK(selectedOverlay.pixelColor(30, 70) == plainOverlay.pixelColor(30, 70));
    key(overlay, Qt::Key_C, Qt::ControlModifier);
    CHECK(QApplication::clipboard()->text() == QStringLiteral("hello world"));
    CHECK(host.isVisible() && overlay.isVisible());
    // Reverse, cross-line drag preserves reading order and original spaces.
    mouse(overlay, QEvent::MouseButtonPress, {30, 70}, Qt::LeftButton, Qt::LeftButton);
    mouse(overlay, QEvent::MouseButtonRelease, {30, 30}, Qt::LeftButton, Qt::NoButton);
    CHECK(overlay.selectedText() == QStringLiteral("hello world\n你"));
    key(overlay, Qt::Key_A, Qt::ControlModifier);
    CHECK(overlay.selectedText() == QStringLiteral("hello world\n你好"));
    key(overlay, Qt::Key_Escape);
    CHECK(!overlay.isVisible() && host.isVisible());
    overlay.setResult(result, QSize(100, 60));
    host.resize(400, 240);
    QApplication::processEvents();
    CHECK(overlay.size() == host.size());
    mouse(overlay, QEvent::MouseButtonPress, {60, 60}, Qt::LeftButton, Qt::LeftButton);
    mouse(overlay, QEvent::MouseButtonRelease, {60, 60}, Qt::LeftButton, Qt::NoButton);
    CHECK(overlay.selectedText() == QStringLiteral("hello"));
    overlay.reset();
    CHECK(!overlay.isVisible() && overlay.selectedText().isEmpty());

    // Rendering and hit testing must use the same image viewport, including
    // offsets and fractional scaling rather than the surrounding widget bounds.
    overlay.setResult(result, QSize(100, 60));
    overlay.setImageDisplayRect(QRectF(7, 9, 150, 90));
    mouse(overlay, QEvent::MouseButtonPress, {31, 31}, Qt::LeftButton, Qt::LeftButton);
    mouse(overlay, QEvent::MouseButtonRelease, {31, 31}, Qt::LeftButton, Qt::NoButton);
    CHECK(overlay.selectedText() == QStringLiteral("hello"));
    const QImage offsetSelection = overlay.grab().toImage();
    CHECK(offsetSelection.pixelColor(31, 31) != offsetSelection.pixelColor(3, 31));
    mouse(overlay, QEvent::MouseButtonPress, {3, 31}, Qt::LeftButton, Qt::LeftButton);
    mouse(overlay, QEvent::MouseButtonRelease, {3, 31}, Qt::LeftButton, Qt::NoButton);
    CHECK(overlay.selectedText().isEmpty());
    overlay.setImageDisplayRect(QRectF());
    overlay.reset();

    // Exercise the actual copy action with disjoint OCR blocks on one row.
    overlay.setResult(splitRows, QSize(200, 120));
    key(overlay, Qt::Key_A, Qt::ControlModifier);
    key(overlay, Qt::Key_C, Qt::ControlModifier);
    CHECK(QApplication::clipboard()->text() == QStringLiteral("left right\nnext"));
    overlay.reset();

    // Exercise the complete async job -> poll -> result path against a local mock.
    QTcpServer server;
    CHECK(server.listen(QHostAddress::LocalHost));
    const QString base = QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort());
    int requests = 0;
    bool wordBoxesRequested = false;
    bool unexpectedRequest = false;
    QByteArray responseResult = fixture;
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&]() {
        while (server.hasPendingConnections()) {
            auto* socket = server.nextPendingConnection();
            QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [&, socket]() {
                if (socket->property("handled").toBool()) return;
                const QByteArray bytes = socket->property("bytes").toByteArray() + socket->readAll();
                socket->setProperty("bytes", bytes);
                const int headerEnd = bytes.indexOf("\r\n\r\n");
                if (headerEnd < 0) return;
                int length = 0;
                for (const auto& header : bytes.left(headerEnd).split('\n')) {
                    if (header.toLower().startsWith("content-length:")) length = header.mid(15).trimmed().toInt();
                }
                if (bytes.size() < headerEnd + 4 + length) return;
                socket->setProperty("handled", true);
                ++requests;
                QByteArray body;
                if (bytes.startsWith("POST /jobs ")) {
                    wordBoxesRequested = bytes.contains("\"returnWordBox\":true");
                    body = R"({"code":0,"data":{"jobId":"1"}})";
                } else if (bytes.startsWith("GET /jobs/1 ")) {
                    const QJsonObject url{{"jsonUrl", base + QStringLiteral("/result")}};
                    body = QJsonDocument(QJsonObject{{"code", 0}, {"data", QJsonObject{{"state", "done"}, {"resultUrl", url}}}}).toJson();
                } else if (bytes.startsWith("GET /result ")) body = responseResult;
                else unexpectedRequest = true;
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: "
                              + QByteArray::number(body.size()) + "\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
        }
    });
    QImage image(100, 60, QImage::Format_RGB32);
    image.fill(Qt::white);
    image.setDevicePixelRatio(2);
    PaddleOcrConfig config{true, base + QStringLiteral("/jobs"), QStringLiteral("test"), QStringLiteral("PP-OCRv6")};
    QString status;
    bool error = false;
    QObject::connect(&overlay, &OcrSelectionOverlay::statusMessage, &host, [&](const QString& text, bool failed) {
        status = text;
        error = failed;
    });
    QApplication::clipboard()->setText(QStringLiteral("keep clipboard"));
    overlay.recognize(image, config);
    CHECK(overlay.isVisible() && overlay.isRecognizing());
    const QImage firstFrame = overlay.grab().toImage();
    QElapsedTimer animation;
    animation.start();
    CHECK(waitFor([&]() { return animation.elapsed() >= 150; }));
    CHECK(overlay.isRecognizing() && firstFrame != overlay.grab().toImage());
    key(overlay, Qt::Key_C, Qt::ControlModifier);
    CHECK(QApplication::clipboard()->text() == QStringLiteral("keep clipboard"));
    overlay.recognize(image, config); // Repeated clicks must not submit duplicate jobs.
    CHECK(waitFor([&]() { return (overlay.isVisible() && !overlay.isRecognizing()) || error; }));
    CHECK(!error && overlay.isVisible() && wordBoxesRequested && !unexpectedRequest && requests == 3);
    CHECK(status.contains(QStringLiteral("Ctrl+C")));
    CHECK(QApplication::clipboard()->text() == QStringLiteral("keep clipboard"));
    overlay.leave();
    overlay.recognize(image, config);
    CHECK(overlay.isVisible() && requests == 3); // Cached results need no new request.
    overlay.reset();
    responseResult = R"({"rec_texts":["no coordinates"]})";
    overlay.recognize(image, config);
    CHECK(waitFor([&]() { return error; }));
    CHECK(!overlay.isVisible() && !overlay.isRecognizing() && status.contains(QStringLiteral("坐标")));
    CHECK(QApplication::clipboard()->text() == QStringLiteral("keep clipboard"));
    error = false;
    overlay.recognize(image, config);
    CHECK(overlay.isRecognizing());
    key(overlay, Qt::Key_Escape); // Canceling invalidates an outstanding OCR request.
    QApplication::processEvents();
    CHECK(!overlay.isVisible() && !overlay.isRecognizing() && overlay.selectedText().isEmpty());
    config.enabled = false;
    error = false;
    overlay.recognize(image, config);
    CHECK(error && !overlay.isVisible() && !overlay.isRecognizing());
    CHECK(QApplication::clipboard()->text() == QStringLiteral("keep clipboard"));
    return 0;
}
