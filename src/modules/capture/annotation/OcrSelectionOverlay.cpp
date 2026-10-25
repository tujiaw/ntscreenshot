#include "OcrSelectionOverlay.h"
#include "core/network/PaddleOcrClient.h"

#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QLinearGradient>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <limits>

namespace {

QPolygonF selectedLinePolygon(const OcrResult& result, int first, int last, const QTransform& transform)
{
    // Project the selected word boxes onto their text baseline. One filled
    // polygon covers inter-word gaps without repeatedly blending overlapping boxes.
    const QPolygonF initial = transform.map(result.spans[first].polygon);
    const QPointF edge = initial[1] - initial[0];
    const qreal length = std::hypot(edge.x(), edge.y());
    const QPointF along = length > 0 ? edge / length : QPointF(1, 0);
    const QPointF across(-along.y(), along.x());
    qreal left = std::numeric_limits<qreal>::max();
    qreal top = left;
    qreal right = std::numeric_limits<qreal>::lowest();
    qreal bottom = right;
    for (int i = first; i <= last; ++i) {
        for (const QPointF& point : transform.map(result.spans[i].polygon)) {
            const qreal x = QPointF::dotProduct(point, along);
            const qreal y = QPointF::dotProduct(point, across);
            left = std::min(left, x);
            right = std::max(right, x);
            top = std::min(top, y);
            bottom = std::max(bottom, y);
        }
    }
    // Add a display-pixel margin at each end, independent of image zoom/DPR.
    left -= 1;
    right += 1;
    return QPolygonF{along * left + across * top, along * right + across * top,
                     along * right + across * bottom, along * left + across * bottom};
}

} // namespace

OcrSelectionOverlay::OcrSelectionOverlay(QWidget* parent, bool fillParent)
    : QWidget(parent), fillParent_(fillParent)
{
    setObjectName(QStringLiteral("OcrSelectionOverlay"));
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_NoMousePropagation);
    scanTimer_.setInterval(16);
    connect(&scanTimer_, &QTimer::timeout, this, qOverload<>(&QWidget::update));
    parent->installEventFilter(this);
    hide();
}

void OcrSelectionOverlay::recognize(const QImage& image, const PaddleOcrConfig& config)
{
    if (client_) return;
    if (source_ == image && !result_.spans.isEmpty()) {
        activate();
        return;
    }
    reset();
    source_ = image;
    imageSize_ = image.size();
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG")) {
        reportStatus(QCoreApplication::translate("App", "截图数据为空"), true);
        return;
    }
    auto* client = new PaddleOcrClient(this);
    client_ = client;
    connect(client, &PaddleOcrClient::succeeded, this, [this, client](const OcrResult& result) {
        if (client_ != client) return;
        client_ = nullptr;
        scanTimer_.stop();
        client->deleteLater();
        if (result.spans.isEmpty()) {
            reportStatus(QCoreApplication::translate("App", "OCR 未返回文字坐标，无法进入选字模式"), true);
            return;
        }
        result_ = result;
        activate();
    });
    connect(client, &PaddleOcrClient::failed, this, [this, client](const QString& error) {
        if (client_ != client) return;
        client_ = nullptr;
        scanTimer_.stop();
        client->deleteLater();
        reportStatus(QCoreApplication::translate("App", "OCR 失败：%1").arg(error), true);
    });
    scanElapsed_.start();
    scanTimer_.start();
    setCursor(Qt::BusyCursor);
    reportStatus(QCoreApplication::translate("App", "正在识别文字，按 Esc 取消"));
    client->recognize(png, config);
}

void OcrSelectionOverlay::showOverlay()
{
    if (fillParent_) setGeometry(parentWidget()->rect());
    show();
    raise();
    setFocus(Qt::OtherFocusReason);
    update();
}

void OcrSelectionOverlay::reportStatus(const QString& message, bool error)
{
    if (error) {
        leave();
    } else {
        setAttribute(Qt::WA_NoMousePropagation, !isRecognizing());
        showOverlay();
        if (!isRecognizing()) setCursor(Qt::ArrowCursor);
    }
    // Keep status available to callers without drawing text over the image.
    emit statusMessage(message, error);
}

void OcrSelectionOverlay::activate()
{
    scanTimer_.stop();
    anchor_ = extent_ = -1;
    dragging_ = false;
    reportStatus(QCoreApplication::translate("App", "已进入选字模式，拖选文字后按 Ctrl+C 复制，Esc 退出"));
}

void OcrSelectionOverlay::setResult(const OcrResult& result, const QSize& imageSize)
{
    reset();
    result_ = result;
    imageSize_ = imageSize;
    activate();
}

void OcrSelectionOverlay::reset()
{
    if (client_) {
        auto* client = client_.data();
        client_ = nullptr;
        client->disconnect(this);
        delete client;
    }
    result_ = {};
    source_ = {};
    leave();
}

void OcrSelectionOverlay::leave()
{
    if (client_) {
        auto* client = client_.data();
        client_ = nullptr;
        client->disconnect(this);
        delete client;
    }
    scanTimer_.stop();
    anchor_ = extent_ = -1;
    dragging_ = false;
    hide();
    parentWidget()->setFocus(Qt::OtherFocusReason);
}

bool OcrSelectionOverlay::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == parentWidget()) {
        if (event->type() == QEvent::Resize && fillParent_) setGeometry(parentWidget()->rect());
        if (event->type() == QEvent::Hide) reset();
    }
    return QWidget::eventFilter(watched, event);
}

void OcrSelectionOverlay::setImageDisplayRect(const QRectF& rect)
{
    imageDisplayRect_ = rect;
    update();
}

QRectF OcrSelectionOverlay::imageDisplayRect() const
{
    return imageDisplayRect_.isValid() ? imageDisplayRect_ : QRectF(rect());
}

QTransform OcrSelectionOverlay::imageTransform() const
{
    const QRectF display = imageDisplayRect();
    QTransform transform;
    transform.translate(display.x(), display.y());
    transform.scale(display.width() / std::max(1, imageSize_.width()),
                    display.height() / std::max(1, imageSize_.height()));
    return transform;
}

QPointF OcrSelectionOverlay::imagePoint(const QPointF& point) const
{
    return imageTransform().inverted().map(point);
}

int OcrSelectionOverlay::hitTest(const QPointF& position, bool nearest) const
{
    if (imageSize_.isEmpty() || (!nearest && !imageDisplayRect().contains(position))) return -1;
    const QPointF point = imagePoint(position);
    int best = -1;
    double distance = std::numeric_limits<double>::max();
    for (int i = 0; i < result_.spans.size(); ++i) {
        const auto& polygon = result_.spans[i].polygon;
        if (polygon.containsPoint(point, Qt::OddEvenFill)) return i;
        if (nearest) {
            const QRectF box = polygon.boundingRect();
            const double dx = std::max({box.left() - point.x(), 0.0, point.x() - box.right()});
            const double dy = std::max({box.top() - point.y(), 0.0, point.y() - box.bottom()});
            if (dx * dx + dy * dy < distance) {
                distance = dx * dx + dy * dy;
                best = i;
            }
        }
    }
    return best;
}

QString OcrSelectionOverlay::selectedText() const
{
    return result_.selectedText(anchor_, extent_);
}

void OcrSelectionOverlay::copySelection()
{
    const QString text = selectedText();
    if (text.isEmpty()) return;
    QApplication::clipboard()->setText(text);
    reportStatus(QCoreApplication::translate("App", "所选文字已复制到剪切板"));
}

void OcrSelectionOverlay::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF display = imageDisplayRect();
    painter.setClipRect(display);
    if (isRecognizing()) {
        painter.translate(display.topLeft());
        painter.fillRect(QRectF(QPointF(0, 0), display.size()), QColor(8, 18, 36, 30));
        const qreal progress = (scanElapsed_.elapsed() % 1800) / 1800.0;
        const qreal y = progress * display.height();
        const qreal trail = std::min(100.0, display.height() * 0.35);
        QLinearGradient gradient(0, y - trail, 0, y);
        gradient.setColorAt(0, QColor(65, 160, 255, 0));
        gradient.setColorAt(1, QColor(65, 160, 255, 90));
        painter.fillRect(QRectF(0, y - trail, display.width(), trail), gradient);
        painter.setPen(QPen(QColor(100, 205, 255, 55), 8));
        painter.drawLine(QPointF(0, y), QPointF(display.width(), y));
        painter.setPen(QPen(QColor(150, 225, 255, 240), 2));
        painter.drawLine(QPointF(0, y), QPointF(display.width(), y));
        painter.setPen(QPen(QColor(85, 175, 255, 220), 2));
        const qreal corner = std::min({20.0, display.width() / 4.0, display.height() / 4.0});
        for (const QPointF& point : {QPointF(2, 2), QPointF(display.width() - 2, 2),
                                     QPointF(2, display.height() - 2), QPointF(display.width() - 2, display.height() - 2)}) {
            painter.drawLine(point, point + QPointF(point.x() < display.width() / 2.0 ? corner : -corner, 0));
            painter.drawLine(point, point + QPointF(0, point.y() < display.height() / 2.0 ? corner : -corner));
        }
    } else if (anchor_ >= 0 && extent_ >= 0 && !imageSize_.isEmpty()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(45, 125, 230, 90));
        const int first = std::min(anchor_, extent_);
        const int last = std::max(anchor_, extent_);
        const QTransform transform = imageTransform();
        for (int i = first; i <= last;) {
            int end = i;
            while (end < last && result_.spans[end + 1].line == result_.spans[i].line) ++end;
            painter.drawPolygon(selectedLinePolygon(result_, i, end, transform));
            i = end + 1;
        }
    }
}

void OcrSelectionOverlay::mousePressEvent(QMouseEvent* event)
{
    if (isRecognizing()) { event->ignore(); return; }
    if (event->button() == Qt::LeftButton) {
        setFocus();
        anchor_ = extent_ = hitTest(event->position());
        dragging_ = anchor_ >= 0;
        update();
    }
    event->accept();
}

void OcrSelectionOverlay::mouseMoveEvent(QMouseEvent* event)
{
    if (isRecognizing()) { event->ignore(); return; }
    setCursor(hitTest(event->position()) >= 0 ? Qt::IBeamCursor : Qt::ArrowCursor);
    if (dragging_) {
        extent_ = hitTest(event->position(), true);
        update();
    }
    event->accept();
}

void OcrSelectionOverlay::mouseReleaseEvent(QMouseEvent* event)
{
    if (isRecognizing()) { event->ignore(); return; }
    if (event->button() == Qt::LeftButton && dragging_) {
        extent_ = hitTest(event->position(), true);
        dragging_ = false;
        update();
    }
    event->accept();
}

void OcrSelectionOverlay::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (isRecognizing()) { event->ignore(); return; }
    const int hit = hitTest(event->position());
    if (hit >= 0) {
        anchor_ = extent_ = hit;
        while (anchor_ > 0 && result_.spans[anchor_ - 1].line == result_.spans[hit].line) --anchor_;
        while (extent_ + 1 < result_.spans.size() && result_.spans[extent_ + 1].line == result_.spans[hit].line) ++extent_;
        dragging_ = false;
        update();
    }
    event->accept();
}

bool OcrSelectionOverlay::handleKey(QKeyEvent* event)
{
    if (!isVisible()) return false;
    if (event->key() == Qt::Key_Escape) leave();
    else if (isRecognizing()) { event->accept(); return true; }
    else if (event->matches(QKeySequence::Copy)) copySelection();
    else if (event->matches(QKeySequence::SelectAll)) {
        anchor_ = result_.spans.isEmpty() ? -1 : 0;
        extent_ = result_.spans.size() - 1;
        update();
    }
    event->accept();
    return true;
}

void OcrSelectionOverlay::keyPressEvent(QKeyEvent* event)
{
    handleKey(event);
}

void OcrSelectionOverlay::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);
    auto* copy = menu.addAction(QCoreApplication::translate("App", "复制"), this, &OcrSelectionOverlay::copySelection);
    copy->setEnabled(!isRecognizing() && !selectedText().isEmpty());
    auto* selectAll = menu.addAction(QCoreApplication::translate("App", "全选"), this, [this]() {
        anchor_ = result_.spans.isEmpty() ? -1 : 0;
        extent_ = result_.spans.size() - 1;
        update();
    });
    selectAll->setEnabled(!isRecognizing() && !result_.spans.isEmpty());
    menu.addSeparator();
    menu.addAction(QCoreApplication::translate("App", "退出选字模式"), this, &OcrSelectionOverlay::leave);
    menu.exec(event->globalPos());
}

void OcrSelectionOverlay::wheelEvent(QWheelEvent* event)
{
    // Let the image host resize the display while OCR coordinates stay in image pixels.
    event->ignore();
}
