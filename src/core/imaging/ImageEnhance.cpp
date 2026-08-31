#include "ImageEnhance.h"
#include "CvBridge.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/photo.hpp>

namespace ImageEnhance {

namespace {

cv::Mat toLabLChannel(const cv::Mat& bgr, cv::Mat& lab)
{
    cv::cvtColor(bgr, lab, cv::COLOR_BGR2Lab);
    std::vector<cv::Mat> channels;
    cv::split(lab, channels);
    return channels[0];
}

QImage fromBgr(const cv::Mat& bgr)
{
    return CvBridge::BgrToQImage(bgr);
}

QImage brighten(const cv::Mat& bgr)
{
    cv::Mat out;
    bgr.convertTo(out, -1, 1.0, 30);
    return fromBgr(out);
}

QImage contrast(const cv::Mat& bgr)
{
    cv::Mat lab;
    cv::Mat l = toLabLChannel(bgr, lab);
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
    cv::Mat l2;
    clahe->apply(l, l2);
    std::vector<cv::Mat> channels;
    cv::split(lab, channels);
    channels[0] = l2;
    cv::Mat merged;
    cv::merge(channels, merged);
    cv::Mat out;
    cv::cvtColor(merged, out, cv::COLOR_Lab2BGR);
    return fromBgr(out);
}

QImage sharpen(const cv::Mat& bgr)
{
    cv::Mat blurred;
    cv::GaussianBlur(bgr, blurred, cv::Size(0, 0), 1.2);
    cv::Mat out;
    cv::addWeighted(bgr, 1.5, blurred, -0.5, 0, out);
    return fromBgr(out);
}

QImage denoise(const cv::Mat& bgr)
{
    cv::Mat out;
    cv::fastNlMeansDenoisingColored(bgr, out, 6.0f, 6.0f, 7, 21);
    return fromBgr(out);
}

QImage autoEnhance(const cv::Mat& bgr)
{
    // Mild CLAHE + light unsharp — safe default for screenshots.
    QImage contrasted = contrast(bgr);
    cv::Mat mid = CvBridge::QImageToBgr(contrasted);
    if (mid.empty()) {
        return contrasted;
    }
    return sharpen(mid);
}

} // namespace

QImage apply(const QImage& image, Preset preset)
{
    if (image.isNull()) {
        return {};
    }
    cv::Mat bgr = CvBridge::QImageToBgr(image);
    if (bgr.empty()) {
        return image;
    }

    switch (preset) {
    case Preset::Brighten:
        return brighten(bgr);
    case Preset::Contrast:
        return contrast(bgr);
    case Preset::Sharpen:
        return sharpen(bgr);
    case Preset::Denoise:
        return denoise(bgr);
    case Preset::Auto:
    default:
        return autoEnhance(bgr);
    }
}

} // namespace ImageEnhance
