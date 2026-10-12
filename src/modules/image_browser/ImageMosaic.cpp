#include "ImageMosaic.h"
#include "core/imaging/CvBridge.h"
#include <QPainterPath>
#include <QPainterPathStroker>
#include <cmath>
#include <new>

namespace ImageBrowser {
namespace {
QImage tile(const QImage& source, QRect region, const EditOperation& op) {
    QImage result(region.size(), QImage::Format_ARGB32);
    if (result.isNull()) throw std::bad_alloc();
    if (op.variant == 2) { result.fill(op.color); return result; }
    const int strength = qBound(2, op.strength, 100);
    if (op.variant == 1) {
        const int radius = qBound(1, strength, 30);
        const QRect halo = region.adjusted(-radius, -radius, radius, radius).intersected(source.rect());
        const QImage sample = source.copy(halo).convertToFormat(QImage::Format_ARGB32_Premultiplied);
        if (sample.isNull()) throw std::bad_alloc();
        cv::Mat blurred;
        cv::GaussianBlur(CvBridge::QImageToMat(sample), blurred, cv::Size(radius*2+1, radius*2+1), 0, 0, cv::BORDER_REPLICATE);
        const QImage converted = QImage(blurred.data, blurred.cols, blurred.rows, int(blurred.step),
            QImage::Format_ARGB32_Premultiplied).convertToFormat(QImage::Format_ARGB32);
        if (converted.isNull()) throw std::bad_alloc();
        result = converted.copy(QRect(region.topLeft()-halo.topLeft(), region.size()));
    }
    for (int y = 0; y < region.height(); ++y) {
        auto* row = reinterpret_cast<QRgb*>(result.scanLine(y));
        const auto* original = reinterpret_cast<const QRgb*>(source.constScanLine(region.y()+y));
        const int sampleY = qMin(source.height()-1, ((region.y()+y)/strength)*strength+strength/2);
        for (int x = 0; x < region.width(); ++x) {
            const int imageX = region.x()+x;
            const int sampleX = qMin(source.width()-1, (imageX/strength)*strength+strength/2);
            const QRgb rgb = op.variant == 1 ? row[x] : source.pixel(sampleX, sampleY);
            row[x] = (rgb & 0x00ffffff) | (original[imageX] & 0xff000000);
        }
    }
    return result;
}
}
void paintMosaic(QPainter& painter, const QImage& source, const EditOperation& op) {
    if (source.isNull()) return;
    auto paintPath = [&](const QPainterPath& mask) {
        const QRect bounds = mask.boundingRect().toAlignedRect().intersected(source.rect());
        painter.save();
        painter.setClipPath(mask, Qt::IntersectClip);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        // Local tiles bound memory even for a stroke across a 72 MP image.
        for (int y = bounds.top(); y <= bounds.bottom(); y += 256) {
            for (int x = bounds.left(); x <= bounds.right(); x += 256) {
                const QRect region(x, y, qMin(256, bounds.right()-x+1), qMin(256, bounds.bottom()-y+1));
                if (mask.intersects(QRectF(region))) painter.drawImage(region.topLeft(), tile(source, region, op));
            }
        }
        painter.restore();
    };
    if (!op.directDrawing) {
        QPainterPath mask; mask.addRect(op.rect.intersected(source.rect())); paintPath(mask); return;
    }
    const qreal diameter = qBound<qreal>(.5, op.brushDiameter, 2*std::hypot(source.width(), source.height()));
    QPainterPathStroker stroker;
    stroker.setWidth(diameter); stroker.setCapStyle(Qt::RoundCap); stroker.setJoinStyle(Qt::RoundJoin);
    for (int i = 0; i < op.brushPoints.size(); ++i) {
        QPainterPath path;
        if (!i || op.brushPoints[i] == op.brushPoints[i-1])
            path.addEllipse(op.brushPoints[i], diameter/2, diameter/2);
        else {
            path.moveTo(op.brushPoints[i-1]); path.lineTo(op.brushPoints[i]); path = stroker.createStroke(path);
        }
        paintPath(path);
    }
}
}
