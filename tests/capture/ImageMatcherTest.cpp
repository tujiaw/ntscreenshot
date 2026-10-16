#include <QCoreApplication>
#include <QImage>
#include <QDebug>
#include <cstdio>
#include "core/imaging/ImageMatcher.h"
#include "core/imaging/ScrollStitcher.h"

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
    for (int shift : {80, 137, -80, -137, -320, 320}) {
        const int start = shift < 0 ? -shift : 0;
        auto [found, score] = ImageMatcher::estimateScrollShift(
            frameAt(page, start), frameAt(page, start + shift), -375, 375);
        if (std::abs(found - shift) > 2 || score >= 0.15) {
            std::fprintf(stderr, "Wrong scroll shift: expected=%d found=%d score=%f\n", shift, found, score);
            return 1;
        }
    }
    for (bool reverse : {false, true}) {
        const QImage unrelated = frameAt(makePage(7), 80);
        auto [falseShift, falseScore] = ImageMatcher::estimateScrollShift(
            reverse ? unrelated : first, reverse ? first : unrelated, -375, 375);
        if (std::abs(falseShift) >= 10 && falseScore < 0.15) {
            std::fprintf(stderr, "Accepted unrelated content: shift=%d score=%f\n", falseShift, falseScore);
            return 1;
        }
    }
    auto [stationaryShift, stationaryScore] = ImageMatcher::estimateScrollShift(first, first, -375, 375);
    if (std::abs(stationaryShift) >= 10 && stationaryScore < 0.15) return 1;
    for (int shift : {225, -225}) {
        const int start = shift < 0 ? -shift : 0;
        auto [found, score] = ImageMatcher::estimateScrollShift(
            page.copy(0, start, page.width(), 300),
            page.copy(0, start + shift, page.width(), 300), -225, 225);
        if (found != shift || score >= 0.15) {
            std::fprintf(stderr, "Small viewport scroll mismatch: expected=%d found=%d\n", shift, found);
            return 1;
        }
    }

    QImage sparse(360, 1000, QImage::Format_RGB32);
    sparse.fill(Qt::white);
    for (int y = 140; y < sparse.height(); ++y) {
        unsigned value = unsigned(y) * 747796405u;
        value ^= value >> 16;
        const int gray = 30 + int(value % 170);
        for (int x = 40; x < 320; ++x) sparse.setPixel(x, y, qRgb(gray, gray, gray));
    }
    for (int shift : {80, -80, 320, -320}) {
        const int start = shift < 0 ? -shift : 0;
        auto [found, score] = ImageMatcher::estimateScrollShift(
            frameAt(sparse, start), frameAt(sparse, start + shift), -375, 375);
        if (found != shift || score >= 0.15) {
            std::fprintf(stderr, "Sparse-page scroll mismatch: expected=%d found=%d\n", shift, found);
            return 1;
        }
    }

    // Use real estimates to stitch a sequence that changes direction and crosses
    // both previous extremes. Every output pixel must match the original page.
    const QVector<int> path = {250, 330, 467, 330, 250, 113, 33, 170, 307, 444};
    QVector<ScrollCaptureFrame> frames;
    QImage previous;
    for (int position : path) {
        const QImage current = page.copy(0, position, page.width(), 500);
        auto match = previous.isNull() ? std::pair<int, double>{0, 0.0}
            : ImageMatcher::estimateScrollShift(previous, current, -375, 375);
        if (match.second >= 0.15) return 1;
        frames.push_back({current, match.first, match.second});
        previous = current;
    }
    if (ScrollStitcher::stitch(frames) != page.copy(0, 33, page.width(), 934)) {
        std::fprintf(stderr, "Matched bidirectional sequence has missing, repeated or misaligned pixels\n");
        return 1;
    }
    return 0;
}
