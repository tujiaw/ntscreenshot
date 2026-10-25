#pragma once

#include <QColor>
#include <QImage>
#include <QRect>
#include <QVector>

namespace ImageUtil {

// FNV-1a 64-bit hash over the canonical 32-bit ARGB pixel data of an image.
// Identical images produce identical hashes.
quint64 PixelHash(const QImage& image);

// Scale an image so its longest edge does not exceed maxEdge (preserving aspect).
// If maxEdge <= 0 the image is returned unchanged.
QImage ScaleToMaxEdge(const QImage& image, int maxEdge);

// Detect non-near-white content bounding box (padding included).
QRect DetectContentRect(const QImage& image, int whiteThreshold = 245);

// Crop to content rect; returns original if detection fails.
QImage AutoCrop(const QImage& image, int whiteThreshold = 245);

// Independently detect transparent or nearly uniform opaque borders on each edge.
// Includes 2px padding; preserves edges whose border/content is uncertain.
QRect DetectContentRectAdaptive(const QImage& image);
QImage AutoCropAdaptive(const QImage& image);

// Extract dominant colors (up to maxColors), sorted by frequency.
QVector<QColor> DominantColors(const QImage& image, int maxColors = 5);

// Similarity in [0,1] using color histogram correlation (1 = identical distribution).
double Similarity(const QImage& a, const QImage& b);

} // namespace ImageUtil
