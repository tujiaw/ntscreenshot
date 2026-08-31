#pragma once

#include <QImage>

namespace ImageEnhance {

enum class Preset {
    Auto = 0,
    Brighten,
    Contrast,
    Sharpen,
    Denoise,
};

QImage apply(const QImage& image, Preset preset);

} // namespace ImageEnhance
