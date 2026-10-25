#pragma once

#include <QPainter>
#include <QPixmap>
#include <QtMath>

namespace CaptureCursor {

// Render at the target physical DPI instead of scaling a small raster asset.
// An odd canvas keeps the hotspot on the same central pixel at every scale.
inline QPixmap crosshair(qreal scale)
{
    scale = qMax<qreal>(1.0, scale);
    const int radius = qCeil(13 * scale);
    QPixmap pixmap(radius * 2 + 1, radius * 2 + 1);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(radius + 0.5, radius + 0.5);
    const QPointF directions[] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    const QColor tips[] = {QColor("#4285FF"), QColor("#00C878"),
                           QColor("#FFB000"), QColor("#FF3B86")};
    auto arms = [&](const QColor& color, qreal thickness) {
        painter.setPen(QPen(color, thickness * scale, Qt::SolidLine, Qt::FlatCap));
        for (const QPointF& direction : directions)
            painter.drawLine(direction * (3 * scale), direction * (11 * scale));
    };
    arms(QColor("#202124"), 3);
    arms(Qt::white, 1);
    for (int i = 0; i < 4; ++i) {
        painter.setPen(QPen(tips[i], 2 * scale, Qt::SolidLine, Qt::FlatCap));
        painter.drawLine(directions[i] * (5 * scale), directions[i] * (11 * scale));
    }
    return pixmap;
}

inline QPixmap brush(int diameter, const QColor& color, qreal scale)
{
    scale = qMax<qreal>(1.0, scale);
    const qreal radius = qMax(1, diameter) / 2.0;
    const int halfCanvas = qCeil(radius + 3 * scale);
    QPixmap pixmap(halfCanvas * 2 + 1, halfCanvas * 2 + 1);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    const QPointF center(halfCanvas + 0.5, halfCanvas + 0.5);
    painter.setBrush(Qt::white);
    painter.drawEllipse(center, radius + 2 * scale, radius + 2 * scale);
    painter.setBrush(QColor("#202124"));
    painter.drawEllipse(center, radius + scale, radius + scale);
    painter.setBrush(color);
    painter.drawEllipse(center, radius, radius);
    return pixmap;
}

} // namespace CaptureCursor
