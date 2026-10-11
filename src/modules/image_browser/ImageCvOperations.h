#pragma once
#include "ImageDocument.h"
namespace ImageBrowser {
ImageResult applyCvOperation(const QImage& image, const EditOperation& operation);
}
