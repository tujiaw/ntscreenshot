#pragma once

#include <QtGlobal>

namespace ImageLimits {
constexpr qint64 MaxEditablePixels = 72000000;
// A 72 MP, 64-bit RGBA image needs about 550 MiB before processing.
constexpr int DecodeAllocationMiB = 1024;
}
