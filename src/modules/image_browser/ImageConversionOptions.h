#pragma once

#include <QSize>

namespace ImageBrowser {
struct ImageConversionOptions {
    QSize outputSize;
    int quality = 95;
    int icoSize = 256;
    int pngCompression = 60;
    bool grayscale = false;
    bool pdfA4 = false;
    // 0: automatic, 1: portrait, 2: landscape.
    int pdfOrientation = 0;
    int pdfMarginMm = 10;
};
}
