#include "core/imaging/ImageUtil.h"

#include <QCoreApplication>
#include <QImage>

#include <cstdio>

namespace {

bool expectRect(const char* name, const QImage& image, const QRect& expected)
{
    const QRect actual = ImageUtil::DetectContentRectAdaptive(image);
    const QImage cropped = ImageUtil::AutoCropAdaptive(image);
    if (actual != expected || cropped.size() != expected.size()) {
        std::fprintf(stderr, "%s: rect (%d,%d %dx%d), size %dx%d; expected (%d,%d %dx%d)\n",
                     name, actual.x(), actual.y(), actual.width(), actual.height(),
                     cropped.width(), cropped.height(), expected.x(), expected.y(),
                     expected.width(), expected.height());
        return false;
    }
    return true;
}

void fillRect(QImage& image, const QRect& rect, QRgb color)
{
    for (int y = rect.top(); y <= rect.bottom(); ++y) {
        for (int x = rect.left(); x <= rect.right(); ++x) {
            image.setPixel(x, y, color);
        }
    }
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const QRect content(8, 7, 24, 16);
    const QRect expected(6, 5, 28, 20); // Existing 2px padding.

    QImage white(40, 32, QImage::Format_RGB32);
    white.fill(Qt::white);
    fillRect(white, content, qRgb(20, 30, 40));
    if (!expectRect("white border", white, expected)) return 1;
    if (ImageUtil::DetectContentRect(white) != expected ||
        ImageUtil::AutoCrop(white).size() != expected.size()) return 2;

    QImage black(40, 32, QImage::Format_RGB32);
    black.fill(Qt::black);
    fillRect(black, content, qRgb(70, 110, 160));
    if (!expectRect("black border", black, expected)) return 3;
    // The legacy white-only path must retain its old behavior for AI processing.
    if (ImageUtil::DetectContentRect(black) != black.rect()) return 4;

    QImage varied(40, 32, QImage::Format_RGB32);
    for (int y = 0; y < varied.height(); ++y) {
        for (int x = 0; x < varied.width(); ++x) {
            const int noise = (x * 7 + y * 11) % 9 - 4;
            varied.setPixel(x, y, qRgb(185 + noise, 198 + noise, 210 + noise));
        }
    }
    fillRect(varied, content, qRgb(35, 60, 95));
    if (!expectRect("near-uniform border", varied, expected)) return 5;

    QImage transparent(40, 32, QImage::Format_ARGB32);
    for (int y = 0; y < transparent.height(); ++y) {
        for (int x = 0; x < transparent.width(); ++x) {
            transparent.setPixel(x, y, qRgba(x * 5 % 256, y * 7 % 256, 42, (x + y) % 17));
        }
    }
    fillRect(transparent, content, qRgba(20, 80, 140, 220));
    if (!expectRect("transparent border", transparent, expected)) return 6;
    const QImage transparentCrop = ImageUtil::AutoCropAdaptive(transparent);
    if (transparentCrop.hasAlphaChannel() == false ||
        transparentCrop.pixel(0, 0) != transparent.pixel(expected.left(), expected.top()) ||
        qAlpha(transparentCrop.pixel(2, 2)) != 220) return 7;

    QImage darkUi(40, 32, QImage::Format_RGB32);
    darkUi.fill(qRgb(25, 30, 38));
    fillRect(darkUi, QRect(0, 0, 40, 5), qRgb(45, 55, 70));
    if (!expectRect("dark UI", darkUi, darkUi.rect())) return 8;

    QImage gradient(40, 32, QImage::Format_RGB32);
    for (int y = 0; y < gradient.height(); ++y) {
        for (int x = 0; x < gradient.width(); ++x) {
            gradient.setPixel(x, y, qRgb(x * 4, y * 4, 80));
        }
    }
    if (!expectRect("gradient edge", gradient, gradient.rect())) return 9;

    QImage edgeContent = white;
    fillRect(edgeContent, QRect(0, 12, 8, 5), qRgb(20, 30, 40));
    if (!expectRect("content touching edge", edgeContent, edgeContent.rect())) return 10;

    QImage uniform(40, 32, QImage::Format_RGB32);
    uniform.fill(qRgb(45, 60, 75));
    if (!expectRect("uniform image", uniform, uniform.rect())) return 11;

    return 0;
}
