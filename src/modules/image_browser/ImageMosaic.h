#pragma once
#include "ImageDocument.h"
#include <QPainter>

namespace ImageBrowser {
// The same source and brush path produce identical preview and history pixels.
void paintMosaic(QPainter& painter, const QImage& source, const EditOperation& operation);
}
