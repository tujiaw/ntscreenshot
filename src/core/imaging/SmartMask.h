#pragma once

#include <QImage>
#include <QRect>
#include <QVector>

namespace SmartMask {

enum class Target {
    Faces = 1,
    TextBlocks = 2,
    All = 3,
};

QVector<QRect> detectRegions(const QImage& image, Target target = Target::All);

// Apply mosaic/blur over detected (or provided) regions. Returns a copy.
QImage applyMosaic(const QImage& image, const QVector<QRect>& regions, int blockSize = 12);
QImage autoMask(const QImage& image, Target target = Target::All, int blockSize = 12);

} // namespace SmartMask
