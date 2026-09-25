#include <QCoreApplication>
#include <QImage>
#include <QDebug>
#include <cstdio>
#include "core/imaging/ImageMatcher.h"

static QImage makePage(int salt)
{
    QImage page(360, 1000, QImage::Format_RGB32);
    page.fill(Qt::white);
    for (int y = 0; y < page.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(page.scanLine(y));
        const int row = y / 24;
        for (int x = 20; x < page.width() - 20; ++x) {
            if (y % 24 == 0 || x == 20 || x == 339) {
                line[x] = qRgb(175, 175, 175);
            } else if (y % 24 >= 7 && y % 24 < 15 &&
                       ((x > 40 && x < 105) || (x > 130 && x < 210)
                        || (x > 235 && x < 315))) {
                unsigned value = unsigned(row * 747796405u) ^ unsigned((x / 5) * 2891336453u)
                    ^ unsigned(salt * 277803737u);
                value ^= value >> 16;
                value *= 2246822519u;
                value ^= value >> 13;
                if (value % 11 < 6) line[x] = qRgb(30 + int(value % 8) * 12, 40, 50);
            }
        }
    }
    return page;
}

static QImage frameAt(const QImage& page, int scroll)
{
    QImage frame = page.copy(0, scroll, 360, 500);
    // Fixed navigation bar: should not dictate the scroll estimate.
    for (int y = 0; y < 28; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(frame.scanLine(y));
        for (int x = 0; x < frame.width(); ++x) {
            line[x] = qRgb(45, 85, 140);
        }
    }
    return frame;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const QImage page = makePage(0);
    const QImage first = frameAt(page, 0);
    for (int shift : {80, 137}) {
        auto [found, score] = ImageMatcher::estimateScrollShift(first, frameAt(page, shift), -10, 375);
        if (std::abs(found - shift) > 2 || score >= 0.15) {
            std::fprintf(stderr, "Wrong scroll shift: expected=%d found=%d score=%f\n", shift, found, score);
            return 1;
        }
    }
    auto [falseShift, falseScore] = ImageMatcher::estimateScrollShift(
        first, frameAt(makePage(7), 80), -10, 375);
    if (falseShift >= 10 && falseScore < 0.15) {
        std::fprintf(stderr, "Accepted unrelated content: shift=%d score=%f\n", falseShift, falseScore);
        return 1;
    }
    return 0;
}
