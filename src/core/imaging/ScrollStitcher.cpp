#include "ScrollStitcher.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace {
// Move the cut into the overlap without blending text. The same search works
// at either edge: direction points from the new content into the existing image.
int seamRows(const QImage& result, const QImage& source, int targetY, int sourceY,
             int direction, int limit)
{
    int bestRows = 0;
    double bestCost = std::numeric_limits<double>::max();
    for (int rows = 1; rows <= qMin(24, limit); ++rows) {
        const int offset = direction < 0 ? -rows : rows - 1;
        const auto* oldLine = reinterpret_cast<const QRgb*>(result.constScanLine(targetY + offset));
        const auto* newLine = reinterpret_cast<const QRgb*>(source.constScanLine(sourceY + offset));
        double difference = 0.0;
        int samples = 0;
        for (int x = result.width() / 10; x < result.width() * 9 / 10; x += 3) {
            difference += std::abs(qGray(oldLine[x]) - qGray(newLine[x]));
            ++samples;
        }
        const double cost = samples ? difference / samples : 255.0;
        if (cost < bestCost) {
            bestCost = cost;
            bestRows = rows;
        }
    }
    return bestCost <= 12.0 ? bestRows : 0;
}
}

QImage ScrollStitcher::stitch(const QVector<ScrollCaptureFrame>& frames, int maxHeight)
{
    if (frames.isEmpty() || frames.first().image.isNull() || maxHeight <= 0) return {};
    const QSize size = frames.first().image.size();
    int position = 0;
    int top = 0;
    int bottom = size.height();
    for (int i = 1; i < frames.size(); ++i) {
        if (frames[i].image.size() != size || std::abs(frames[i].shiftFromPrev) >= size.height()) return {};
        position += frames[i].shiftFromPrev;
        top = qMin(top, position);
        bottom = qMax(bottom, position + size.height());
    }
    const int height = qMin(bottom - top, maxHeight);
    QImage result(size.width(), height, QImage::Format_RGB32);
    if (result.isNull()) return {};
    result.fill(Qt::white);
    const int outputBottom = top + height;
    auto copyRows = [&](const QImage& image, int imageTop, int from, int to) {
        for (int y = qMax(from, top); y < qMin(to, outputBottom); ++y) {
            std::memcpy(result.scanLine(y - top), image.constScanLine(y - imageTop),
                        size.width() * sizeof(QRgb));
        }
    };
    const QImage first = frames.first().image.convertToFormat(QImage::Format_RGB32);
    copyRows(first, 0, 0, size.height());
    int coveredTop = 0;
    int coveredBottom = size.height();
    position = 0;
    for (int i = 1; i < frames.size(); ++i) {
        position += frames[i].shiftFromPrev;
        const int frameBottom = position + size.height();
        const QImage image = frames[i].image.convertToFormat(QImage::Format_RGB32);
        if (position < coveredTop) {
            int seam = 0;
            if (coveredTop >= top && coveredTop < outputBottom) {
                const int overlap = std::min({coveredBottom, frameBottom, outputBottom}) - coveredTop;
                seam = seamRows(result, image, coveredTop - top, coveredTop - position, 1, overlap);
            }
            copyRows(image, position, position, coveredTop + seam);
            coveredTop = position;
        }
        if (frameBottom > coveredBottom) {
            int seam = 0;
            if (coveredBottom > top && coveredBottom <= outputBottom) {
                const int overlap = coveredBottom - std::max({coveredTop, position, top});
                seam = seamRows(result, image, coveredBottom - top, coveredBottom - position, -1, overlap);
            }
            copyRows(image, position, coveredBottom - seam, frameBottom);
            coveredBottom = frameBottom;
        }
    }
    return result;
}
