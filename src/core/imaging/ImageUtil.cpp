#include "ImageUtil.h"
#include "CvBridge.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace ImageUtil {

quint64 PixelHash(const QImage& image) {
    if (image.isNull()) {
        return 0;
    }

    const QImage argb = image.convertToFormat(QImage::Format_ARGB32);
    const int width = argb.width();
    const int height = argb.height();
    if (width == 0 || height == 0) {
        return 0;
    }

    quint64 hash = 14695981039346656037ULL;
    const int stride = argb.bytesPerLine();
    const uchar* bits = argb.constBits();
    for (int y = 0; y < height; ++y) {
        const uchar* row = bits + static_cast<qsizetype>(y) * stride;
        for (int x = 0; x < width; ++x) {
            const uchar* px = row + static_cast<qsizetype>(x) * 4;
            hash ^= px[0];
            hash *= 1099511628211ULL;
            hash ^= px[1];
            hash *= 1099511628211ULL;
            hash ^= px[2];
            hash *= 1099511628211ULL;
            hash ^= px[3];
            hash *= 1099511628211ULL;
        }
    }
    return hash;
}

QImage ScaleToMaxEdge(const QImage& image, int maxEdge) {
    if (image.isNull() || maxEdge <= 0) {
        return image;
    }
    const int w = image.width();
    const int h = image.height();
    if (w <= maxEdge && h <= maxEdge) {
        return image;
    }

    int targetW = w;
    int targetH = h;
    if (w >= h) {
        targetW = maxEdge;
        targetH = qMax(1, h * maxEdge / w);
    } else {
        targetH = maxEdge;
        targetW = qMax(1, w * maxEdge / h);
    }
    return image.scaled(targetW, targetH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QRect DetectContentRect(const QImage& image, int whiteThreshold)
{
    if (image.isNull()) {
        return {};
    }

    const QImage rgbImage = image.convertToFormat(QImage::Format_RGB32);
    const int width = rgbImage.width();
    const int height = rgbImage.height();

    auto rowHasContent = [&](int y) {
        const QRgb* row = reinterpret_cast<const QRgb*>(rgbImage.constScanLine(y));
        for (int x = 0; x < width; ++x) {
            const QRgb pixel = row[x];
            if (qRed(pixel) < whiteThreshold || qGreen(pixel) < whiteThreshold || qBlue(pixel) < whiteThreshold) {
                return true;
            }
        }
        return false;
    };

    auto colHasContent = [&](int x, int top, int bottom) {
        for (int y = top; y <= bottom; ++y) {
            const QRgb* row = reinterpret_cast<const QRgb*>(rgbImage.constScanLine(y));
            const QRgb pixel = row[x];
            if (qRed(pixel) < whiteThreshold || qGreen(pixel) < whiteThreshold || qBlue(pixel) < whiteThreshold) {
                return true;
            }
        }
        return false;
    };

    int top = 0;
    while (top < height && !rowHasContent(top)) {
        ++top;
    }
    if (top >= height) {
        return QRect(0, 0, width, height);
    }

    int bottom = height - 1;
    while (bottom > top && !rowHasContent(bottom)) {
        --bottom;
    }

    int left = 0;
    while (left < width && !colHasContent(left, top, bottom)) {
        ++left;
    }

    int right = width - 1;
    while (right > left && !colHasContent(right, top, bottom)) {
        --right;
    }

    const int padding = 2;
    left = qMax(0, left - padding);
    top = qMax(0, top - padding);
    right = qMin(width - 1, right + padding);
    bottom = qMin(height - 1, bottom + padding);
    return QRect(QPoint(left, top), QPoint(right, bottom));
}

QImage AutoCrop(const QImage& image, int whiteThreshold)
{
    const QRect contentRect = DetectContentRect(image, whiteThreshold);
    if (!contentRect.isValid() ||
        contentRect.width() <= 0 ||
        contentRect.height() <= 0 ||
        contentRect.width() > image.width() ||
        contentRect.height() > image.height()) {
        return image;
    }
    if (contentRect == image.rect()) {
        return image;
    }
    return image.copy(contentRect);
}

QRect DetectContentRectAdaptive(const QImage& image)
{
    if (image.isNull()) {
        return {};
    }

    constexpr int colorTolerance = 12;
    constexpr int transparentThreshold = 16;
    constexpr int opaqueThreshold = 240;
    constexpr int padding = 2;
    const QImage argb = image.convertToFormat(QImage::Format_ARGB32);
    const int width = argb.width();
    const int height = argb.height();
    const QRect fullRect = image.rect();
    if (width < 2 || height < 2) {
        return fullRect;
    }

    const QRgb corners[] = {
        argb.pixel(0, 0), argb.pixel(width - 1, 0),
        argb.pixel(0, height - 1), argb.pixel(width - 1, height - 1)
    };
    const bool transparent = std::all_of(std::begin(corners), std::end(corners),
        [](QRgb pixel) { return qAlpha(pixel) <= transparentThreshold; });
    const bool opaque = std::all_of(std::begin(corners), std::end(corners),
        [](QRgb pixel) { return qAlpha(pixel) >= opaqueThreshold; });
    if (!transparent && !opaque) {
        return fullRect;
    }

    const int red = (qRed(corners[0]) + qRed(corners[1]) + qRed(corners[2]) + qRed(corners[3])) / 4;
    const int green = (qGreen(corners[0]) + qGreen(corners[1]) + qGreen(corners[2]) + qGreen(corners[3])) / 4;
    const int blue = (qBlue(corners[0]) + qBlue(corners[1]) + qBlue(corners[2]) + qBlue(corners[3])) / 4;
    auto matchesBorder = [&](QRgb pixel) {
        return transparent ? qAlpha(pixel) <= transparentThreshold
                           : qAlpha(pixel) >= opaqueThreshold &&
                                 std::abs(qRed(pixel) - red) <= colorTolerance &&
                                 std::abs(qGreen(pixel) - green) <= colorTolerance &&
                                 std::abs(qBlue(pixel) - blue) <= colorTolerance;
    };
    if (opaque && !std::all_of(std::begin(corners), std::end(corners), matchesBorder)) {
        return fullRect;
    }

    // Check each outer edge independently before treating any inward line as border.
    auto rowMatches = [&](int y, int left, int right) {
        const QRgb* row = reinterpret_cast<const QRgb*>(argb.constScanLine(y));
        int mismatches = 0;
        const int count = right - left + 1;
        for (int x = left; x <= right; ++x) {
            if (!matchesBorder(row[x]) && ++mismatches * 100 > count) {
                return false;
            }
        }
        return true;
    };
    auto columnMatches = [&](int x, int top, int bottom) {
        int mismatches = 0;
        const int count = bottom - top + 1;
        for (int y = top; y <= bottom; ++y) {
            const QRgb* row = reinterpret_cast<const QRgb*>(argb.constScanLine(y));
            if (!matchesBorder(row[x]) && ++mismatches * 100 > count) {
                return false;
            }
        }
        return true;
    };
    if (!rowMatches(0, 0, width - 1) || !rowMatches(height - 1, 0, width - 1) ||
        !columnMatches(0, 0, height - 1) || !columnMatches(width - 1, 0, height - 1)) {
        return fullRect;
    }

    int top = 0;
    while (top < height && rowMatches(top, 0, width - 1)) {
        ++top;
    }
    if (top == height) { // Uniform image: no reliable content boundary.
        return fullRect;
    }
    int bottom = height - 1;
    while (bottom > top && rowMatches(bottom, 0, width - 1)) {
        --bottom;
    }
    int left = 0;
    while (left < width && columnMatches(left, top, bottom)) {
        ++left;
    }
    if (left == width) {
        return fullRect;
    }
    int right = width - 1;
    while (right > left && columnMatches(right, top, bottom)) {
        --right;
    }

    return QRect(QPoint(qMax(0, left - padding), qMax(0, top - padding)),
                 QPoint(qMin(width - 1, right + padding), qMin(height - 1, bottom + padding)));
}

QImage AutoCropAdaptive(const QImage& image)
{
    const QRect contentRect = DetectContentRectAdaptive(image);
    return contentRect.isValid() && contentRect != image.rect() ? image.copy(contentRect) : image;
}

QVector<QColor> DominantColors(const QImage& image, int maxColors)
{
    QVector<QColor> colors;
    if (image.isNull() || maxColors <= 0) {
        return colors;
    }

    QImage small = ScaleToMaxEdge(image.convertToFormat(QImage::Format_RGB32), 128);
    cv::Mat bgr = CvBridge::QImageToBgr(small);
    if (bgr.empty()) {
        return colors;
    }

    cv::Mat data = bgr.reshape(1, bgr.rows * bgr.cols);
    data.convertTo(data, CV_32F);

    const int k = std::min(maxColors, data.rows);
    if (k <= 0) {
        return colors;
    }

    cv::Mat labels, centers;
    cv::kmeans(data, k, labels,
               cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 10, 1.0),
               3, cv::KMEANS_PP_CENTERS, centers);

    std::vector<int> counts(static_cast<size_t>(k), 0);
    for (int i = 0; i < labels.rows; ++i) {
        const int label = labels.at<int>(i);
        if (label >= 0 && label < k) {
            counts[static_cast<size_t>(label)]++;
        }
    }

    std::vector<int> order(static_cast<size_t>(k));
    for (int i = 0; i < k; ++i) {
        order[static_cast<size_t>(i)] = i;
    }
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return counts[static_cast<size_t>(a)] > counts[static_cast<size_t>(b)];
    });

    for (int idx : order) {
        const float b = centers.at<float>(idx, 0);
        const float g = centers.at<float>(idx, 1);
        const float r = centers.at<float>(idx, 2);
        colors.push_back(QColor(qBound(0, qRound(r), 255),
                                qBound(0, qRound(g), 255),
                                qBound(0, qRound(b), 255)));
    }
    return colors;
}

double Similarity(const QImage& a, const QImage& b)
{
    if (a.isNull() || b.isNull()) {
        return 0.0;
    }
    if (PixelHash(a) == PixelHash(b) && PixelHash(a) != 0) {
        return 1.0;
    }

    cv::Mat bgrA = CvBridge::QImageToBgr(ScaleToMaxEdge(a, 160));
    cv::Mat bgrB = CvBridge::QImageToBgr(ScaleToMaxEdge(b, 160));
    if (bgrA.empty() || bgrB.empty()) {
        return 0.0;
    }

    cv::Mat hsvA, hsvB;
    cv::cvtColor(bgrA, hsvA, cv::COLOR_BGR2HSV);
    cv::cvtColor(bgrB, hsvB, cv::COLOR_BGR2HSV);

    const int hBins = 30;
    const int sBins = 32;
    const int histSize[] = {hBins, sBins};
    const float hRanges[] = {0, 180};
    const float sRanges[] = {0, 256};
    const float* ranges[] = {hRanges, sRanges};
    const int channels[] = {0, 1};

    cv::Mat histA, histB;
    cv::calcHist(&hsvA, 1, channels, cv::Mat(), histA, 2, histSize, ranges, true, false);
    cv::calcHist(&hsvB, 1, channels, cv::Mat(), histB, 2, histSize, ranges, true, false);
    cv::normalize(histA, histA, 0, 1, cv::NORM_MINMAX);
    cv::normalize(histB, histB, 0, 1, cv::NORM_MINMAX);

    const double corr = cv::compareHist(histA, histB, cv::HISTCMP_CORREL);
    if (std::isnan(corr)) {
        return 0.0;
    }
    if (corr < 0.0) {
        return 0.0;
    }
    if (corr > 1.0) {
        return 1.0;
    }
    return corr;
}

} // namespace ImageUtil
