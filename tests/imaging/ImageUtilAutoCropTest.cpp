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
    if (!expectRect("content touching left edge", edgeContent, QRect(0, 5, 34, 20))) return 10;

    QImage uniform(40, 32, QImage::Format_RGB32);
    uniform.fill(qRgb(45, 60, 75));
    if (!expectRect("uniform image", uniform, uniform.rect())) return 11;

    // All combinations of independently removable edges, with textured content
    // reaching every edge that has no border. Cover white, black and transparent.
    for (int background = 0; background < 3; ++background) {
        for (int edges = 1; edges < 16; ++edges) {
            QImage image(80, 60, QImage::Format_ARGB32);
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    image.setPixel(x, y, ((x / 4 + y / 4) % 2)
                        ? qRgb(40, 70, 110) : qRgb(120, 150, 190));
                }
            }
            const QRgb border = background == 0 ? qRgb(255, 255, 255)
                : background == 1 ? qRgb(0, 0, 0) : qRgba(80, 40, 20, 0);
            if (edges & 1) fillRect(image, QRect(0, 0, 8, 60), border);
            if (edges & 2) fillRect(image, QRect(0, 0, 80, 8), border);
            if (edges & 4) fillRect(image, QRect(72, 0, 8, 60), border);
            if (edges & 8) fillRect(image, QRect(0, 52, 80, 8), border);
            const QRect bounds = image.rect().adjusted(edges & 1 ? 6 : 0, edges & 2 ? 6 : 0,
                                                       edges & 4 ? -6 : 0, edges & 8 ? -6 : 0);
            if (!expectRect("independent borders", image, bounds)) {
                std::fprintf(stderr, "background=%d edges=%d\n", background, edges);
                return 12;
            }
            const QImage cropped = ImageUtil::AutoCropAdaptive(image);
            if (cropped != image.copy(bounds)) return 13;
        }
    }

    QImage cornerMark = white;
    cornerMark.setPixel(0, 0, qRgb(0, 0, 0));
    if (!expectRect("corner mark preserves two edges", cornerMark, QRect(0, 0, 34, 25))) return 14;

    // A tiny foreground mark on a long scanline must not be discarded as noise.
    QImage punctuation(1000, 60, QImage::Format_RGB32);
    punctuation.fill(Qt::white);
    punctuation.setPixel(0, 6, qRgb(0, 0, 0));
    if (!expectRect("single foreground pixel", punctuation, QRect(0, 4, 3, 5))) return 15;

    QImage translucent(40, 32, QImage::Format_ARGB32);
    translucent.fill(qRgba(100, 110, 120, 128));
    fillRect(translucent, content, qRgb(20, 30, 40));
    if (!expectRect("uncertain translucent background", translucent, translucent.rect())) return 16;

    QImage narrow(1, 20, QImage::Format_RGB32);
    narrow.fill(Qt::white);
    if (!expectRect("narrow image", narrow, narrow.rect())) return 17;
    if (ImageUtil::DetectContentRectAdaptive(QImage()).isValid()) return 18;

    white.setDevicePixelRatio(2.0);
    if (ImageUtil::AutoCropAdaptive(white).devicePixelRatio() != 2.0) return 19;

    QImage thinBorder(40, 32, QImage::Format_RGB32);
    thinBorder.fill(Qt::white);
    fillRect(thinBorder, QRect(2, 2, 36, 28), qRgb(20, 30, 40));
    if (!expectRect("keep padding for thin borders", thinBorder, thinBorder.rect())) return 20;

    return 0;
}
