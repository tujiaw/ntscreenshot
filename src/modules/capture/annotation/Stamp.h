#pragma once

#include <QImage>
#include <QPainter>
#include <QPainterPath>

namespace Stamp {
enum Kind { Number, Flag, Heart, Pushpin, MapPin };

inline QImage image(Kind kind, const QColor& color, int number, int size) {
    QImage result(size, size, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(size / 32.0, size / 32.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    QPainterPath path;
    switch (kind) {
    case Number: {
        painter.drawRoundedRect(QRectF(2, 2, 28, 28), 5, 5);
        QFont font(QStringLiteral("Segoe UI"));
        font.setBold(true);
        font.setPixelSize(20);
        const QString label = QString::number(number);
        while (QFontMetricsF(font).horizontalAdvance(label) > 24 && font.pixelSize() > 6)
            font.setPixelSize(font.pixelSize() - 1);
        painter.setFont(font);
        painter.setPen(Qt::white);
        painter.drawText(QRectF(2, 1, 28, 28), Qt::AlignCenter, label);
        break;
    }
    case Flag:
        painter.setPen(QPen(QColor("#C5C8CE"), 2.5, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(7, 3), QPointF(7, 29));
        painter.setPen(Qt::NoPen);
        path.moveTo(8, 4); path.lineTo(28, 11); path.lineTo(8, 19); path.closeSubpath();
        painter.drawPath(path);
        break;
    case Heart:
        path.moveTo(16, 28); path.cubicTo(-8, 12, 6, -3, 16, 8);
        path.cubicTo(26, -3, 40, 12, 16, 28);
        painter.drawPath(path);
        break;
    case Pushpin:
        painter.translate(16, 16); painter.rotate(40);
        painter.setPen(QPen(QColor("#ADB3BD"), 2, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(0, 5), QPointF(0, 16));
        painter.setPen(Qt::NoPen);
        path.moveTo(-7, -12); path.lineTo(7, -12); path.lineTo(7, -8);
        path.lineTo(4, -8); path.lineTo(4, 0); path.lineTo(9, 3);
        path.lineTo(9, 6); path.lineTo(-9, 6); path.lineTo(-9, 3);
        path.lineTo(-4, 0); path.lineTo(-4, -8); path.lineTo(-7, -8); path.closeSubpath();
        painter.drawPath(path);
        break;
    case MapPin:
        painter.setPen(QPen(QColor("#ADB3BD"), 2, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(16, 15), QPointF(16, 29));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QRectF(8, 2, 16, 16));
        painter.setBrush(Qt::white);
        painter.drawEllipse(QRectF(17, 5, 4, 4));
        break;
    }
    return result;
}
}
