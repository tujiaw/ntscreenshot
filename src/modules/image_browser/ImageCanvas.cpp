#include "ImageCanvas.h"
#include "ImageAnnotation.h"
#include "ImageMosaic.h"
#include "core/theme/ThemeManager.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
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
    perspective_.clear(); perspectiveHandle_ = -1;
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
    if (enabled) { perspective_.clear(); perspectiveHandle_ = -1; annotating_ = false; drawing_ = false; mosaicPreview_ = {}; mosaicSource_ = {}; }
    cropAspectRatio_ = 0;
    cropping_ = enabled && !image_.isNull();
    crop_ = {};
    dragging_ = false;
    updateDrawingCursor();
    if (cropping_) setFocus();
    emit selectionChanged();
    update();
}
void ImageCanvas::setCropAspectRatio(double ratio) {
    ratio = std::isfinite(ratio) && ratio > 0 ? ratio : 0;
    if (cropAspectRatio_ == ratio) return;
    cropAspectRatio_ = ratio;
    if (ratio > 0 && !crop_.isEmpty()) {
        const QPointF center = crop_.center();
        const double width = qMin(crop_.width(), crop_.height()*ratio);
        crop_.setSize(QSizeF(width, width/ratio)); crop_.moveCenter(center);
        emit selectionChanged();
    }
    update();
}
int ImageCanvas::cropHandleAt(QPointF point) const {
    if (!cropping_ || crop_.isEmpty()) return 0;
    const QPointF c = crop_.center();
    const QPointF points[] = {crop_.topLeft(), crop_.topRight(), crop_.bottomLeft(), crop_.bottomRight(),
        QPointF(crop_.left(), c.y()), QPointF(crop_.right(), c.y()),
        QPointF(c.x(), crop_.top()), QPointF(c.x(), crop_.bottom())};
    const qreal tolerance = qMax(9, fontMetrics().height()/2);
    for (int i = 0; i < 8; ++i) {
        const QPointF delta = viewportPoint(points[i])-point;
        if (qAbs(delta.x()) <= tolerance && qAbs(delta.y()) <= tolerance) return i < 4 ? i+1 : i+2;
    }
    return crop_.contains(imagePoint(point)) ? 5 : 0;
}
void ImageCanvas::setPerspective(const QPolygonF& corners) {
    const QPolygonF next = !image_.isNull() && corners.size() == 4 ? corners : QPolygonF{};
    if (perspective_ == next) return;
    perspective_ = next;
    if (perspective_.isEmpty()) perspectiveHandle_ = -1;
    updateDrawingCursor(); update();
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
        if (perspective_.size() == 4) {
            QPolygonF polygon;
            for (const auto& point : perspective_) polygon << viewportPoint(point);
            painter.save(); painter.setClipRect(rect().adjusted(leftInset_, 0, 0, 0));
            painter.setRenderHint(QPainter::Antialiasing);
            QPainterPath shade; shade.setFillRule(Qt::OddEvenFill);
            shade.addRect(target); shade.addPolygon(polygon); shade.closeSubpath();
            painter.fillPath(shade, QColor(0, 0, 0, 110));
            painter.setPen(QPen(theme.accent, 2)); painter.setBrush(Qt::NoBrush);
            painter.drawPolygon(polygon);
            painter.setPen(QPen(QColor(255, 255, 255, 160), 1, Qt::DashLine));
            for (int i = 1; i < 3; ++i) {
                const qreal t = i / 3.;
                painter.drawLine(polygon[0]*(1-t)+polygon[1]*t, polygon[3]*(1-t)+polygon[2]*t);
                painter.drawLine(polygon[0]*(1-t)+polygon[3]*t, polygon[1]*(1-t)+polygon[2]*t);
            }
            const qreal radius = qMax(7, fontMetrics().height()/2);
            for (int i = 0; i < 4; ++i) {
                painter.setPen(QPen(Qt::white, 2)); painter.setBrush(theme.accent);
                painter.drawEllipse(polygon[i], radius, radius);
                painter.drawText(QRectF(polygon[i]-QPointF(radius, radius), QSizeF(radius*2, radius*2)), Qt::AlignCenter, QString::number(i+1));
            }
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
            painter.setPen(QPen(QColor(255, 255, 255, 170), 1, Qt::DashLine));
            for (int i = 1; i < 3; ++i) {
                const qreal x = selection.left()+selection.width()*i/3.;
                const qreal y = selection.top()+selection.height()*i/3.;
                painter.drawLine(QPointF(x, selection.top()), QPointF(x, selection.bottom()));
                painter.drawLine(QPointF(selection.left(), y), QPointF(selection.right(), y));
            }
            painter.setPen(QPen(theme.accent, 2)); painter.setBrush(theme.accent);
            const qreal handleSize = qMax(4, fontMetrics().height() / 4);
            for (const QPointF& p : {selection.topLeft(), selection.topRight(), selection.bottomLeft(), selection.bottomRight(),
                QPointF(selection.left(), selection.center().y()), QPointF(selection.right(), selection.center().y()),
                QPointF(selection.center().x(), selection.top()), QPointF(selection.center().x(), selection.bottom())})
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
    if (perspective_.size() == 4) {
        const qreal tolerance = qMax(12, fontMetrics().height());
        for (int i = 0; i < 4; ++i) {
            if (QLineF(viewportPoint(perspective_[i]), event->position()).length() <= tolerance) {
                perspectiveHandle_ = i; setCursor(Qt::ClosedHandCursor); return;
            }
        }
    }
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
        handle_ = cropHandleAt(press_);
        if (!handle_) crop_ = QRectF(imagePos, imagePos);
    } else setCursor(Qt::ClosedHandCursor);
    if (cropping_) emit selectionChanged();
}
void ImageCanvas::mouseMoveEvent(QMouseEvent* event) {
    hoverPoint_ = event->position(); hovering_ = true; updateDrawingCursor();
    if (perspectiveHandle_ >= 0) {
        QPointF point = imagePoint(event->position());
        point.setX(qBound(0., point.x(), double(image_.width()-1)));
        point.setY(qBound(0., point.y(), double(image_.height()-1)));
        point = QPointF(qRound(point.x()), qRound(point.y()));
        perspective_[perspectiveHandle_] = point;
        setCursor(Qt::ClosedHandCursor);
        emit perspectiveChanged(perspective_); update(); return;
    }
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
        } else if (handle_ >= 6) {
            const QPointF center = initialCrop_.center();
            if (handle_ == 6 || handle_ == 7) {
                const double fixed = handle_ == 6 ? initialCrop_.right() : initialCrop_.left();
                double width = qAbs(p.x()-fixed);
                if (cropAspectRatio_ > 0) width = qMin(width, 2*qMin(center.y(), image_.height()-center.y())*cropAspectRatio_);
                const double height = cropAspectRatio_ > 0 ? width/cropAspectRatio_ : initialCrop_.height();
                crop_ = QRectF(p.x() < fixed ? fixed-width : fixed, center.y()-height/2, width, height);
            } else {
                const double fixed = handle_ == 8 ? initialCrop_.bottom() : initialCrop_.top();
                double height = qAbs(p.y()-fixed);
                if (cropAspectRatio_ > 0) height = qMin(height, 2*qMin(center.x(), image_.width()-center.x())/cropAspectRatio_);
                const double width = cropAspectRatio_ > 0 ? height*cropAspectRatio_ : initialCrop_.width();
                crop_ = QRectF(center.x()-width/2, p.y() < fixed ? fixed-height : fixed, width, height);
            }
        } else {
            QPointF anchor = imagePoint(press_);
            if (handle_ == 1) anchor = initialCrop_.bottomRight();
            if (handle_ == 2) anchor = initialCrop_.bottomLeft();
            if (handle_ == 3) anchor = initialCrop_.topRight();
            if (handle_ == 4) anchor = initialCrop_.topLeft();
            anchor.setX(qBound(0.0, anchor.x(), double(image_.width())));
            anchor.setY(qBound(0.0, anchor.y(), double(image_.height())));
            if (cropAspectRatio_ > 0) {
                const double sx = p.x() < anchor.x() ? -1 : 1;
                const double sy = p.y() < anchor.y() ? -1 : 1;
                double width = qMax(qAbs(p.x()-anchor.x()), qAbs(p.y()-anchor.y())*cropAspectRatio_);
                width = qMin(width, sx > 0 ? image_.width()-anchor.x() : anchor.x());
                width = qMin(width, (sy > 0 ? image_.height()-anchor.y() : anchor.y())*cropAspectRatio_);
                p = anchor + QPointF(sx*width, sy*width/cropAspectRatio_);
            }
            crop_ = QRectF(anchor, p).normalized();
        }
    }
    if (cropping_) emit selectionChanged();
    update();
}
void ImageCanvas::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    if (perspectiveHandle_ >= 0) {
        mouseMoveEvent(event); perspectiveHandle_ = -1; updateDrawingCursor(); return;
    }
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
    if (perspective_.size() == 4 && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
        emit perspectiveAccepted(); return;
    }
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
    if (hovering_ && cropping_) {
        const int hit = cropHandleAt(hoverPoint_);
        setCursor(hit == 1 || hit == 4 ? Qt::SizeFDiagCursor : hit == 2 || hit == 3 ? Qt::SizeBDiagCursor :
            hit == 6 || hit == 7 ? Qt::SizeHorCursor : hit == 8 || hit == 9 ? Qt::SizeVerCursor :
            hit == 5 ? Qt::SizeAllCursor : Qt::CrossCursor);
        return;
    }
    if (hovering_ && perspective_.size() == 4) {
        for (const auto& point : perspective_)
            if (QLineF(viewportPoint(point), hoverPoint_).length() <= qMax(12, fontMetrics().height())) {
                setCursor(Qt::SizeAllCursor); return;
            }
    }
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
