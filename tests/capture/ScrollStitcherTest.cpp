#include <QCoreApplication>
#include <cstdio>
#include "core/imaging/ScrollStitcher.h"

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QImage page(120, 2400, QImage::Format_RGB32);
    for (int y = 0; y < page.height(); ++y) {
        for (int x = 0; x < page.width(); ++x) {
            page.setPixel(x, y, qRgb((y * 17 + x) % 256, y % 256, y / 256));
        }
    }
    const QVector<QVector<int>> paths = {
        {600}, {600, 700, 900}, {600, 500, 300},
        {600, 700, 600, 500, 700, 900, 600, 300},
        {600, 500, 600, 700, 600, 500, 600},
        {600, 590, 600, 610, 600},
    };
    for (const auto& path : paths) {
        QVector<ScrollCaptureFrame> frames;
        int previous = path.first();
        int top = previous;
        int bottom = previous + 500;
        for (int position : path) {
            frames.push_back({page.copy(0, position, page.width(), 500), position - previous, 0.0});
            previous = position;
            top = qMin(top, position);
            bottom = qMax(bottom, position + 500);
        }
        const QImage actual = ScrollStitcher::stitch(frames);
        if (actual != page.copy(0, top, page.width(), bottom - top)) {
            std::fprintf(stderr, "Bidirectional stitch differs from page, path length=%lld height=%d expected=%d\n",
                         static_cast<long long>(path.size()), actual.height(), bottom - top);
            return 1;
        }
        const QImage limited = ScrollStitcher::stitch(frames, 550);
        if (limited != page.copy(0, top, page.width(), qMin(550, bottom - top))) {
            std::fprintf(stderr, "Height-limited stitch differs from page\n");
            return 1;
        }
    }
    if (!ScrollStitcher::stitch({}).isNull()) return 1;
    const QImage initial = page.copy(0, 600, page.width(), 500);
    QImage changed = page.copy(0, 700, page.width(), 500);
    changed.fill(Qt::red);
    // A frame revisiting an already covered interval must not replace it, even
    // if a hover or animation has changed its pixels since the first visit.
    const QVector<ScrollCaptureFrame> revisited = {
        {initial, 0, 0.0}, {page.copy(0, 800, page.width(), 500), 200, 0.0},
        {changed, -100, 0.0}
    };
    if (ScrollStitcher::stitch(revisited) != page.copy(0, 600, page.width(), 700)) return 1;
    if (!ScrollStitcher::stitch({{initial, 0, 0.0}, {initial, 500, 0.0}}).isNull()) return 1;
    if (!ScrollStitcher::stitch({{initial, 0, 0.0}}, 0).isNull()) return 1;
    return 0;
}
