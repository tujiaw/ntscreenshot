#pragma once

#include <QtGlobal>

namespace ImageLimits {
constexpr qint64 MaxEditablePixels = 100000000;
// A 100 MP, 64-bit RGBA image needs about 763 MiB before processing.
constexpr int DecodeAllocationMiB = 1024;
}
