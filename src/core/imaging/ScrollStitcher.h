#pragma once

#include <QImage>
#include <QVector>

struct ScrollCaptureFrame {
    QImage image;
    int shiftFromPrev; // Positive scrolls down the page, negative scrolls up.
    double matchScore;
};

namespace ScrollStitcher {
QImage stitch(const QVector<ScrollCaptureFrame>& frames, int maxHeight = 60000);
}
