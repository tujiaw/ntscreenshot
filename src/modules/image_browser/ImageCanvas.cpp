#include "ImageCanvas.h"
#include "ImageAnnotation.h"
#include "ImageMosaic.h"
#include "core/theme/ThemeManager.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <cmath>
#include <new>

namespace ImageBrowser {
ImageCanvas::ImageCanvas(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(160, 120);
    setMouseTracking(true);
}
void ImageCanvas::setImage(const QImage& image, bool resetView) {
    drawing_ = false;
    mosaicPreview_ = {}; mosaicSource_ = {};
    image_ = image;
    message_.clear();
    if (resetView) { setCropping(false); fit(false); }
    update();
}
void ImageCanvas::setMessage(const QString& message) { message_ = message; update(); }
void ImageCanvas::setLeftInset(int inset) {
    inset = qBound(0, inset, qMax(0, width() - 80));
    if (leftInset_ == inset) return;
    const int delta = inset - leftInset_;
    leftInset_ = inset;
    if (fitted_) fit(fitUpscale_);
    else { offset_.rx() += delta / 2.; update(); }
}
QPointF ImageCanvas::imagePoint(QPointF point) const { return (point - offset_) / scale_; }
QPointF ImageCanvas::viewportPoint(QPointF point) const { return point * scale_ + offset_; }
void ImageCanvas::fit(bool allowUpscale) {
    fitted_ = true;
    fitUpscale_ = allowUpscale;
    if (!image_.isNull()) {
        const int margin = qMax(16, fontMetrics().height());
        const double available = qMin(double(qMax(1, width() - leftInset_ - 2 * margin)) / image_.width(),
                                      double(qMax(1, height() - 2 * margin)) / image_.height());
        scale_ = qMin(available, allowUpscale ? 8.0 : 1.0);
        offset_ = QPointF(leftInset_ + (width() - leftInset_ - image_.width() * scale_) / 2, (height() - image_.height() * scale_) / 2);
    }
    emit zoomChanged();
    update();
}
void ImageCanvas::zoomAt(double scale, QPointF anchor) {
    const QPointF location = imagePoint(anchor);
    scale_ = qBound(0.05, scale, 8.0);
    offset_ = anchor - location * scale_;
    updateDrawingCursor();
    fitted_ = false;
    emit zoomChanged();
    update();
}
void ImageCanvas::actualSize() { zoomAt(1, QPointF((width() + leftInset_) / 2., height() / 2.)); }
void ImageCanvas::setZoom(double scale) { zoomAt(scale, QPointF((width() + leftInset_) / 2., height() / 2.)); }
bool ImageCanvas::cancelDrawing() {
    if (!drawing_) return false;
    drawing_ = false; mosaicPreview_ = {}; mosaicSource_ = {}; update(); return true;
}
void ImageCanvas::setAnnotation(const EditOperation& operation, bool enabled) {
    annotation_ = operation;
    const bool active = enabled && !image_.isNull();
    if (annotating_ != active || (drawing_ && stroke_.kind != operation.kind)) {
        if (drawing_) { mosaicPreview_ = {}; mosaicSource_ = {}; }
        drawing_ = false; dragging_ = false;
    }
    annotating_ = active;
    updateDrawingCursor();
    update();
}
void ImageCanvas::setCropping(bool enabled) {
    if (enabled) { annotating_ = false; drawing_ = false; mosaicPreview_ = {}; mosaicSource_ = {}; }
    cropping_ = enabled && !image_.isNull();
    crop_ = {};
    dragging_ = false;
    updateDrawingCursor();
    if (cropping_) setFocus();
    emit selectionChanged();
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
    painter.setClipRect(rect().adjusted(leftInset_ + margin, margin, -margin, -margin));
    if (!image_.isNull()) {
        const QRectF target(offset_, QSizeF(image_.size()) * scale_);
        painter.save();
        painter.setClipRect(target, Qt::IntersectClip);
        if (image_.hasAlphaChannel()) {
            if (checkerboard_.isNull() || checkerColor_ != theme.surface) {
                checkerColor_ = theme.surface;
                checkerboard_ = QImage(24, 24, QImage::Format_RGB32);
                // Light themes may use the same color for surface and raised surface.
                checkerboard_.fill(theme.surface.lightness() > 128 ? theme.surface.darker(112) : theme.surface.lighter(145));
                QPainter tile(&checkerboard_);
                tile.fillRect(0, 0, 12, 12, theme.surface);
                tile.fillRect(12, 12, 12, 12, theme.surface);
            }
            painter.fillRect(target, QBrush(checkerboard_));
        }
        painter.setRenderHint(QPainter::SmoothPixmapTransform, scale_ < 1);
        painter.drawImage(target, mosaicPreview_.isNull() ? image_ : mosaicPreview_);
        painter.restore();
        if (drawing_ && stroke_.kind != EditOperation::Mosaic) {
            painter.save(); painter.setClipRect(target, Qt::IntersectClip);
            painter.translate(offset_); painter.scale(scale_, scale_);
            paintAnnotation(painter, stroke_); painter.restore();
        }
        if (annotating_ && annotation_.kind == EditOperation::Mosaic && hovering_) {
            const qreal radius = qBound<qreal>(8, annotation_.brushDiameter, 500)/2;
            painter.save(); painter.setClipRect(target, Qt::IntersectClip);
            painter.setRenderHint(QPainter::Antialiasing); painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(Qt::black, 3)); painter.drawEllipse(hoverPoint_, radius, radius);
            painter.setPen(QPen(Qt::white, 1)); painter.drawEllipse(hoverPoint_, radius, radius);
            painter.restore();
        }
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
    if (drawing_) { event->accept(); return; }
    if (!image_.isNull()) zoomAt(scale_ * std::pow(1.15, event->angleDelta().y() / 120.0), event->position());
    event->accept();
}
void ImageCanvas::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton || image_.isNull()) return;
    setFocus();
    hoverPoint_ = event->position(); hovering_ = true; updateDrawingCursor();
    if (annotating_) {
        const QPointF point = imagePoint(event->position());
        const int margin = qMax(16, fontMetrics().height());
        if (!rect().adjusted(leftInset_ + margin, margin, -margin, -margin).contains(event->position().toPoint()) ||
            !QRectF(image_.rect()).contains(point)) return;
        stroke_ = annotation_; stroke_.directDrawing = true; stroke_.start = stroke_.end = point;
        stroke_.brushPoints = {point};
        drawing_ = true;
        if (stroke_.kind == EditOperation::Mosaic) {
            stroke_.brushDiameter = qBound<qreal>(8, annotation_.brushDiameter, 500)/scale_;
            mosaicSource_ = image_.convertToFormat(QImage::Format_ARGB32);
            mosaicPreview_ = mosaicSource_; previewMosaic({point});
        }
        update(); return;
    }
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
    if (cropping_) emit selectionChanged();
}
void ImageCanvas::mouseMoveEvent(QMouseEvent* event) {
    hoverPoint_ = event->position(); hovering_ = true; updateDrawingCursor();
    if (annotating_ && annotation_.kind == EditOperation::Mosaic) update();
    if (drawing_) {
        QPointF point = imagePoint(event->position());
        if (stroke_.kind != EditOperation::Mosaic) {
            point.setX(qBound(0., point.x(), double(image_.width()-1)));
            point.setY(qBound(0., point.y(), double(image_.height()-1)));
        }
        if (stroke_.kind == EditOperation::Mosaic && QLineF(stroke_.end, point).length() >= .5) {
            const QPointF previous = stroke_.end;
            stroke_.brushPoints << point;
            previewMosaic({previous, point});
        }
        stroke_.end = point; update(); return;
    }
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
    if (cropping_) emit selectionChanged();
    update();
}
void ImageCanvas::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    if (drawing_) {
        mouseMoveEvent(event);
        drawing_ = false; update();
        // Retain the painted pixels until the asynchronous document commit
        // calls setImage(), avoiding a flash of the unedited source on release.
        mosaicSource_ = {};
        if (stroke_.kind == EditOperation::Mosaic || stroke_.variant == 3 || QLineF(stroke_.start, stroke_.end).length() >= 1)
            emit annotationAccepted(stroke_);
        return;
    }
    dragging_ = false;
    updateDrawingCursor();
}
void ImageCanvas::keyPressEvent(QKeyEvent* event) {
    if (annotating_ && event->key() == Qt::Key_Escape) {
        cancelDrawing(); return;
    }
    if (cropping_ && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
        if (!cropRect().isEmpty()) emit cropAccepted(cropRect());
    } else if (cropping_ && event->key() == Qt::Key_Escape) {
        setCropping(false); emit cropCancelled();
    } else if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Up) emit navigate(-1);
    else if (event->key() == Qt::Key_Right || event->key() == Qt::Key_Down) emit navigate(1);
    else QWidget::keyPressEvent(event);
}

void ImageCanvas::updateDrawingCursor() {
    const bool brush = annotating_ && annotation_.kind == EditOperation::Mosaic;
    const int margin = qMax(16, fontMetrics().height());
    const bool inside = hovering_ && QRectF(image_.rect()).contains(imagePoint(hoverPoint_)) &&
        rect().adjusted(leftInset_+margin, margin, -margin, -margin).contains(hoverPoint_.toPoint());
    setCursor(brush ? (inside ? Qt::BlankCursor : Qt::ArrowCursor)
        : (annotating_ || cropping_) ? Qt::CrossCursor : Qt::OpenHandCursor);
}

void ImageCanvas::leaveEvent(QEvent* event) {
    hovering_ = false; updateDrawingCursor(); update(); QWidget::leaveEvent(event);
}

void ImageCanvas::previewMosaic(const QPolygonF& points) {
    try {
        EditOperation segment = stroke_; segment.brushPoints = points;
        QPainter painter(&mosaicPreview_);
        if (!painter.isActive()) throw std::bad_alloc();
        paintMosaic(painter, mosaicSource_, segment);
    } catch (const std::exception&) {
        drawing_ = false; mosaicPreview_ = {}; mosaicSource_ = {};
        // Keep the last committed image intact if a preview allocation fails.
        setMessage(tr("图片处理失败或内存不足。"));
    }
}
} // namespace ImageBrowser
