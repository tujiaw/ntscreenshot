#pragma once
#include "ImageDocument.h"
#include <QPainter>
#include <QLineF>
#include <QFontMetricsF>
#include <cmath>

namespace ImageBrowser {
// Shared by the transient canvas preview and the committed document render.
inline void paintAnnotation(QPainter& painter, const EditOperation& operation) {
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    const int width = qBound(1, operation.strength, 100);
    painter.setPen(QPen(operation.color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    const QRectF bounds = operation.directDrawing
        ? QRectF(operation.start, operation.end).normalized()
        : QRectF(operation.rect).adjusted(width/2., width/2., -width/2., -width/2.);
    if (operation.variant == 3) {
        QFont font = painter.font(); font.setPixelSize(qBound(8, operation.strength, 200));
        painter.setFont(font); painter.setPen(operation.color);
        if (operation.directDrawing) {
            const QFontMetricsF metrics(font);
            const QRectF textBounds = metrics.boundingRect(QRectF(0, 0, 100000, 100000), Qt::AlignLeft | Qt::AlignTop, operation.text);
            painter.drawText(QRectF(operation.start, textBounds.size()), Qt::AlignLeft | Qt::AlignTop, operation.text);
        } else painter.drawText(operation.rect, Qt::AlignCenter | Qt::TextWordWrap, operation.text);
    } else if (operation.variant == 0) painter.drawRect(bounds);
    else if (operation.variant == 1) painter.drawEllipse(bounds);
    else {
        const QPointF start = operation.directDrawing ? operation.start : bounds.bottomLeft();
        const QPointF end = operation.directDrawing ? operation.end : bounds.topRight();
        painter.drawLine(start, end);
        const double angle = std::atan2(end.y()-start.y(), end.x()-start.x());
        const double head = qMin(QLineF(start, end).length()/2, qMax(12., width*4.));
        painter.drawLine(end, end - QPointF(std::cos(angle-.5)*head, std::sin(angle-.5)*head));
        painter.drawLine(end, end - QPointF(std::cos(angle+.5)*head, std::sin(angle+.5)*head));
    }
    painter.restore();
}
} // namespace ImageBrowser
