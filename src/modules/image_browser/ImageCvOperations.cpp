#include "ImageCvOperations.h"
#include "core/imaging/CvBridge.h"
#include <QCoreApplication>
#include <cmath>
#include <cstdint>
#include <new>

namespace ImageBrowser {
namespace {
QString message(const char* text) { return QCoreApplication::translate("ImageBrowser", text); }
struct RandomState {
    std::uint64_t state = cv::theRNG().state;
    RandomState() { cv::theRNG().state = 0x4e545343; }
    ~RandomState() { cv::theRNG().state = state; }
};
ImageResult cutout(const QImage& image, const EditOperation& operation) {
    ImageResult result;
    const QRect region = operation.rect.intersected(image.rect());
    if (region.width() < 2 || region.height() < 2 || region == image.rect()) {
        result.error = message("请框选完整主体，并在选区外留出部分背景。"); return result;
    }
    // GrabCut's graph is bounded independently of the original image size.
    const QImage sample = qMax(image.width(), image.height()) > 1280
        ? image.scaled(1280, 1280, Qt::KeepAspectRatio, Qt::SmoothTransformation) : image;
    if (sample.isNull()) throw std::bad_alloc();
    const double sx = double(sample.width())/image.width(), sy = double(sample.height())/image.height();
    const int left = qBound(0, int(std::floor(region.x()*sx)), sample.width()-1);
    const int top = qBound(0, int(std::floor(region.y()*sy)), sample.height()-1);
    const int right = qBound(left, int(std::ceil((region.x()+region.width())*sx)), sample.width());
    const int bottom = qBound(top, int(std::ceil((region.y()+region.height())*sy)), sample.height());
    const qint64 foregroundSamples = qint64(right-left)*(bottom-top);
    const qint64 backgroundSamples = qint64(sample.width())*sample.height()-foregroundSamples;
    if (right-left < 2 || bottom-top < 2 || foregroundSamples < 5 || backgroundSamples < 5) {
        result.error = message("请框选完整主体，并在选区外留出部分背景。"); return result;
    }
    cv::Mat mask(sample.height(), sample.width(), CV_8UC1, cv::Scalar(cv::GC_BGD));
    cv::Mat background, foreground;
    {
        RandomState random;
        cv::grabCut(CvBridge::QImageToBgr(sample), mask, cv::Rect(left, top, right-left, bottom-top),
            background, foreground, qBound(1, operation.strength, 5), cv::GC_INIT_WITH_RECT);
    }
    mask = (mask == cv::GC_FGD) | (mask == cv::GC_PR_FGD);
    if (cv::countNonZero(mask) == 0) {
        result.error = message("未识别到主体，请重新选择主体区域。"); return result;
    }
    cv::Mat fullMask;
    const int feather = qBound(0, operation.feather, 10);
    cv::resize(mask, fullMask, cv::Size(image.width(), image.height()), 0, 0, feather ? cv::INTER_LINEAR : cv::INTER_NEAREST);
    if (feather) cv::GaussianBlur(fullMask, fullMask, cv::Size(feather*2+1, feather*2+1), 0);
    result.image = image;
    for (int y = 0; y < image.height(); ++y) {
        auto* row = reinterpret_cast<QRgb*>(result.image.scanLine(y));
        const auto* alpha = fullMask.ptr<uchar>(y);
        for (int x = 0; x < image.width(); ++x) row[x] = (row[x] & 0x00ffffff) | (uint((qAlpha(row[x])*alpha[x]+127)/255) << 24);
    }
    return result;
}
}
ImageResult applyCvOperation(const QImage& image, const EditOperation& operation) {
    if (operation.kind == EditOperation::Cutout) return cutout(image, operation);
    ImageResult result;
    if (operation.variant < 3 || operation.variant > 8) {
        result.error = message("图片处理参数无效。"); return result;
    }
    result.image = QImage(image.size(), QImage::Format_ARGB32);
    if (result.image.isNull()) throw std::bad_alloc();
    if (operation.variant == 6 || operation.variant == 7) {
        cv::Mat gray, output;
        cv::cvtColor(CvBridge::QImageToMat(image), gray, cv::COLOR_BGRA2GRAY);
        if (operation.variant == 6) {
            cv::GaussianBlur(gray, gray, cv::Size(3, 3), 0);
            const int threshold = qBound(1, operation.strength, 255);
            cv::Canny(gray, output, threshold, threshold*2);
        } else cv::threshold(gray, output, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
        for (int y = 0; y < image.height(); ++y) {
            const auto* source = reinterpret_cast<const QRgb*>(image.constScanLine(y));
            auto* target = reinterpret_cast<QRgb*>(result.image.scanLine(y));
            const auto* values = output.ptr<uchar>(y);
            for (int x = 0; x < image.width(); ++x) target[x] = qRgba(values[x], values[x], values[x], qAlpha(source[x]));
        }
        return result;
    }
    int minimum = 255, maximum = 0;
    if (operation.variant == 8) {
        for (int y = 0; y < image.height(); ++y) {
            const auto* row = reinterpret_cast<const QRgb*>(image.constScanLine(y));
            for (int x = 0; x < image.width(); ++x) if (qAlpha(row[x])) {
                minimum = qMin(minimum, qMin(qRed(row[x]), qMin(qGreen(row[x]), qBlue(row[x]))));
                maximum = qMax(maximum, qMax(qRed(row[x]), qMax(qGreen(row[x]), qBlue(row[x]))));
            }
        }
    }
    const int radius = operation.variant == 4 ? qBound(1, operation.strength, 4) : qBound(1, operation.strength, 20);
    const int halo = operation.variant == 5 ? 3 : radius;
    // Process neighborhoods in tiles, with overlap to prevent seams and bounded temporary memory.
    for (int y = 0; y < image.height(); y += 512) for (int x = 0; x < image.width(); x += 512) {
        const QRect tile = QRect(x, y, 512, 512).intersected(image.rect());
        const QRect extended = tile.adjusted(-halo, -halo, halo, halo).intersected(image.rect());
        QImage sample = image.copy(extended);
        if (operation.variant != 8) sample = sample.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        cv::Mat processed;
        const cv::Mat input = CvBridge::QImageToMat(sample);
        if (operation.variant == 3) cv::GaussianBlur(input, processed, cv::Size(radius*2+1, radius*2+1), 0);
        else if (operation.variant == 4) cv::medianBlur(input, processed, radius*2+1);
        else if (operation.variant == 5) {
            cv::Mat blurred; cv::GaussianBlur(input, blurred, cv::Size(7, 7), 0);
            const double amount = qBound(1, operation.strength, 20)/10.;
            cv::addWeighted(input, 1+amount, blurred, -amount, 0, processed);
        } else {
            const double scale = maximum > minimum ? 255./(maximum-minimum) : 1.;
            input.convertTo(processed, -1, scale, maximum > minimum ? -minimum*scale : 0);
        }
        QImage converted(processed.data, processed.cols, processed.rows, int(processed.step),
            operation.variant == 8 ? QImage::Format_ARGB32 : QImage::Format_ARGB32_Premultiplied);
        const QImage straight = converted.convertToFormat(QImage::Format_ARGB32);
        const QPoint offset = tile.topLeft()-extended.topLeft();
        for (int row = 0; row < tile.height(); ++row) {
            const auto* original = reinterpret_cast<const QRgb*>(image.constScanLine(y+row));
            const auto* filtered = reinterpret_cast<const QRgb*>(straight.constScanLine(offset.y()+row));
            auto* target = reinterpret_cast<QRgb*>(result.image.scanLine(y+row));
            for (int col = 0; col < tile.width(); ++col) target[x+col] = (filtered[offset.x()+col] & 0x00ffffff) | (original[x+col] & 0xff000000);
        }
    }
    return result;
}
} // namespace ImageBrowser
