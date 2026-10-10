#include "ImageCanvas.h"
#include "core/theme/ThemeManager.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <cmath>

namespace ImageBrowser {
ImageCanvas::ImageCanvas(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(160, 120);
    setMouseTracking(true);
}
void ImageCanvas::setImage(const QImage& image, bool resetView) {
    image_ = image;
    message_.clear();
    if (resetView) { setCropping(false); fit(false); }
    update();
}
void ImageCanvas::setMessage(const QString& message) { message_ = message; update(); }
QPointF ImageCanvas::imagePoint(QPointF point) const { return (point - offset_) / scale_; }
QPointF ImageCanvas::viewportPoint(QPointF point) const { return point * scale_ + offset_; }
void ImageCanvas::fit(bool allowUpscale) {
    fitted_ = true;
    fitUpscale_ = allowUpscale;
    if (!image_.isNull()) {
        const int margin = qMax(16, fontMetrics().height());
        const double available = qMin(double(qMax(1, width() - 2 * margin)) / image_.width(),
                                      double(qMax(1, height() - 2 * margin)) / image_.height());
        scale_ = qMin(available, allowUpscale ? 8.0 : 1.0);
        offset_ = QPointF((width() - image_.width() * scale_) / 2, (height() - image_.height() * scale_) / 2);
    }
    emit zoomChanged();
    update();
}
void ImageCanvas::zoomAt(double scale, QPointF anchor) {
    const QPointF location = imagePoint(anchor);
    scale_ = qBound(0.05, scale, 8.0);
    offset_ = anchor - location * scale_;
    fitted_ = false;
    emit zoomChanged();
    update();
}
void ImageCanvas::actualSize() { zoomAt(1, rect().center()); }
void ImageCanvas::setCropping(bool enabled) {
    cropping_ = enabled && !image_.isNull();
    crop_ = {};
    dragging_ = false;
    setCursor(cropping_ ? Qt::CrossCursor : Qt::OpenHandCursor);
    if (cropping_) setFocus();
    update();
}
QRect ImageCanvas::cropRect() const {
    const QRectF area = crop_.normalized().intersected(QRectF(image_.rect()));
    const int left = qMax(0, int(std::floor(area.left() + 1e-7)));
    const int top = qMax(0, int(std::floor(area.top() + 1e-7)));
    const int right = qMin(image_.width(), int(std::ceil(area.right() - 1e-7)));
    const int bottom = qMin(image_.height(), int(std::ceil(area.bottom() - 1e-7)));
    return QRect(left, top, qMax(0, right - left), qMax(0, bottom - top));
}
void ImageCanvas::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    const auto& theme = ThemeManager::tokens();
    painter.fillRect(rect(), theme.canvas);
    const int margin = qMax(16, fontMetrics().height());
    painter.setClipRect(rect().adjusted(margin, margin, -margin, -margin));
    if (!image_.isNull()) {
        const QRectF target(offset_, QSizeF(image_.size()) * scale_);
        painter.save();
        painter.setClipRect(target, Qt::IntersectClip);
        if (image_.hasAlphaChannel()) {
            if (checkerboard_.isNull() || checkerColor_ != theme.surface) {
                checkerColor_ = theme.surface;
                checkerboard_ = QImage(24, 24, QImage::Format_RGB32);
                checkerboard_.fill(theme.surfaceRaised);
                QPainter tile(&checkerboard_);
                tile.fillRect(0, 0, 12, 12, theme.surface);
                tile.fillRect(12, 12, 12, 12, theme.surface);
            }
            painter.fillRect(target, QBrush(checkerboard_));
        }
        painter.setRenderHint(QPainter::SmoothPixmapTransform, scale_ < 1);
        painter.drawImage(target, image_);
        painter.restore();
        if (cropping_ && !crop_.isEmpty()) {
            const QRectF selection(viewportPoint(crop_.topLeft()), viewportPoint(crop_.bottomRight()));
            QRegion outside(rect());
            outside -= selection.toAlignedRect();
            painter.save();
            painter.setClipRegion(outside, Qt::IntersectClip);
            painter.fillRect(rect(), QColor(0, 0, 0, 110));
            painter.restore();
            painter.setPen(QPen(theme.accent, 2));
            painter.drawRect(selection);
            painter.setBrush(theme.accent);
            const qreal handleSize = qMax(4, fontMetrics().height() / 4);
            for (const QPointF& p : {selection.topLeft(), selection.topRight(), selection.bottomLeft(), selection.bottomRight()})
                painter.drawRect(QRectF(p - QPointF(handleSize, handleSize), QSizeF(handleSize * 2, handleSize * 2)));
        }
    }
    if (!message_.isEmpty()) {
        painter.fillRect(rect(), theme.canvas);
        painter.setPen(theme.textSecondary);
        painter.drawText(rect().adjusted(24, 24, -24, -24), Qt::AlignCenter | Qt::TextWordWrap, message_);
    }
}
void ImageCanvas::resizeEvent(QResizeEvent*) { if (fitted_) fit(fitUpscale_); emit viewportChanged(); }
void ImageCanvas::wheelEvent(QWheelEvent* event) {
    if (!image_.isNull()) zoomAt(scale_ * std::pow(1.15, event->angleDelta().y() / 120.0), event->position());
    event->accept();
}
void ImageCanvas::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton || image_.isNull()) return;
    setFocus();
    dragging_ = true;
    press_ = event->position();
    initialOffset_ = offset_;
    initialCrop_ = crop_;
    handle_ = 0;
    if (cropping_) {
        const QPointF imagePos = imagePoint(press_);
        const QPointF corners[] = {crop_.topLeft(), crop_.topRight(), crop_.bottomLeft(), crop_.bottomRight()};
        if (!crop_.isEmpty()) {
            for (int i = 0; i < 4; ++i) {
                const QPointF delta = viewportPoint(corners[i]) - press_;
                const qreal tolerance = qMax(9, fontMetrics().height() / 2);
                if (qAbs(delta.x()) <= tolerance && qAbs(delta.y()) <= tolerance) { handle_ = i + 1; break; }
            }
            if (!handle_ && crop_.contains(imagePos)) handle_ = 5;
        }
        if (!handle_) crop_ = QRectF(imagePos, imagePos);
    } else setCursor(Qt::ClosedHandCursor);
}
void ImageCanvas::mouseMoveEvent(QMouseEvent* event) {
    if (!dragging_) return;
    if (!cropping_) { fitted_ = false; offset_ = initialOffset_ + event->position() - press_; }
    else {
        QPointF p = imagePoint(event->position());
        p.setX(qBound(0.0, p.x(), double(image_.width())));
        p.setY(qBound(0.0, p.y(), double(image_.height())));
        if (handle_ == 5) {
            crop_ = initialCrop_.translated((event->position() - press_) / scale_);
            crop_.moveLeft(qBound(0.0, crop_.left(), image_.width() - crop_.width()));
            crop_.moveTop(qBound(0.0, crop_.top(), image_.height() - crop_.height()));
        } else {
            QPointF anchor = imagePoint(press_);
            if (handle_ == 1) anchor = initialCrop_.bottomRight();
            if (handle_ == 2) anchor = initialCrop_.bottomLeft();
            if (handle_ == 3) anchor = initialCrop_.topRight();
            if (handle_ == 4) anchor = initialCrop_.topLeft();
            anchor.setX(qBound(0.0, anchor.x(), double(image_.width())));
            anchor.setY(qBound(0.0, anchor.y(), double(image_.height())));
            crop_ = QRectF(anchor, p).normalized();
        }
    }
    update();
}
void ImageCanvas::mouseReleaseEvent(QMouseEvent*) {
    dragging_ = false;
    setCursor(cropping_ ? Qt::CrossCursor : Qt::OpenHandCursor);
}
void ImageCanvas::keyPressEvent(QKeyEvent* event) {
    if (cropping_ && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
        if (!cropRect().isEmpty()) emit cropAccepted(cropRect());
    } else if (cropping_ && event->key() == Qt::Key_Escape) {
        setCropping(false); emit cropCancelled();
    } else if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Up) emit navigate(-1);
    else if (event->key() == Qt::Key_Right || event->key() == Qt::Key_Down) emit navigate(1);
    else QWidget::keyPressEvent(event);
}
} // namespace ImageBrowser
