#pragma once

#include "core/theme/ThemeManager.h"
#include <QIconEngine>
#include <QPainter>
#include <QPainterPath>

// A single stroke grid keeps toolbar symbols consistent at every DPI.
namespace AnnotationIcon {
class Engine : public QIconEngine {
public:
    Engine(QString name, bool primary) : name_(std::move(name)), primary_(primary) {}
    QIconEngine* clone() const override { return new Engine(*this); }
    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override
    {
        const auto& tokens = ThemeManager::tokens();
        const QColor color = mode == QIcon::Disabled ? tokens.textDisabled
            : primary_ ? QColor(Qt::white) : state == QIcon::On ? tokens.accent : tokens.textPrimary;
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const qreal size = qMin(rect.width(), rect.height());
        painter->translate(rect.center().x() - size / 2, rect.center().y() - size / 2);
        painter->scale(size / 24, size / 24);
        painter->setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(Qt::NoBrush);
        auto line = [&](qreal x1, qreal y1, qreal x2, qreal y2) {
            painter->drawLine(QPointF(x1, y1), QPointF(x2, y2));
        };
        if (name_ == "color.png") {
            painter->drawEllipse(QRectF(4, 4, 16, 16));
            painter->setPen(Qt::NoPen);
            for (const QPointF& point : {QPointF(9, 8), QPointF(15, 8), QPointF(8, 14), QPointF(15, 15)}) {
                painter->setBrush(color); painter->drawEllipse(point, 1.4, 1.4);
            }
        } else if (name_ == "polyline.png") {
            QPainterPath path; path.moveTo(5, 19); path.lineTo(7, 14); path.lineTo(16, 5);
            path.lineTo(19, 8); path.lineTo(10, 17); path.closeSubpath();
            painter->drawPath(path); line(14, 7, 17, 10);
        } else if (name_ == "line.png") {
            line(5, 19, 19, 5);
        } else if (name_ == "arrow.png") {
            line(4, 12, 20, 12); line(14, 6, 20, 12); line(14, 18, 20, 12);
        } else if (name_ == "rectangle.png") {
            painter->drawRoundedRect(QRectF(4, 5, 16, 14), 1.5, 1.5);
        } else if (name_ == "ellipse.png") {
            painter->drawEllipse(QRectF(3, 6, 18, 12));
        } else if (name_ == "text.png") {
            line(5, 5, 19, 5); line(12, 5, 12, 19); line(9, 19, 15, 19);
        } else if (name_ == "mosaic.png") {
            for (int y = 0; y < 3; ++y) for (int x = 0; x < 3; ++x)
                painter->drawRoundedRect(QRectF(4 + x * 6, 4 + y * 6, 3, 3), 0.5, 0.5);
        } else if (name_ == "undo.png") {
            line(8, 5, 3, 10); line(3, 10, 8, 15);
            QPainterPath path; path.moveTo(3, 10); path.lineTo(13, 10);
            path.cubicTo(19, 10, 21, 16, 18, 19); painter->drawPath(path);
        } else if (name_ == "pin.png") {
            painter->drawRoundedRect(QRectF(4, 4, 16, 16), 2, 2);
            line(14, 20, 14, 14); line(14, 14, 20, 14);
        } else if (name_ == "save.png") {
            line(12, 3, 12, 15); line(7, 10, 12, 15); line(12, 15, 17, 10);
            QPainterPath path; path.moveTo(4, 15); path.lineTo(4, 20);
            path.lineTo(20, 20); path.lineTo(20, 15); painter->drawPath(path);
        } else if (name_ == "clipboard.png") {
            painter->drawRoundedRect(QRectF(8, 7, 12, 14), 2, 2);
            QPainterPath path; path.moveTo(16, 3); path.lineTo(5, 3);
            path.quadTo(3, 3, 3, 5); path.lineTo(3, 16); painter->drawPath(path);
        } else if (name_ == "ocr") {
            for (int x : {4, 20}) for (int y : {4, 20}) {
                line(x, y, x + (x == 4 ? 4 : -4), y);
                line(x, y, x, y + (y == 4 ? 4 : -4));
            }
            line(8, 9, 16, 9); line(12, 9, 12, 16);
        } else {
            painter->setPen(Qt::NoPen); painter->setBrush(color);
            for (int x : {5, 12, 19}) painter->drawEllipse(QPointF(x, 12), 1.7, 1.7);
        }
        painter->restore();
    }
    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override
    {
        QPixmap result(size); result.fill(Qt::transparent);
        QPainter painter(&result); paint(&painter, result.rect(), mode, state); return result;
    }
private:
    QString name_;
    bool primary_;
};
inline QIcon icon(const QString& name, bool primary = false) { return QIcon(new Engine(name, primary)); }
} // namespace AnnotationIcon
