#pragma once

#include <QImage>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

namespace CvBridge {

inline cv::Mat QImageToMat(const QImage& inImage, bool clone = false)
{
    switch (inImage.format()) {
    case QImage::Format_ARGB32:
    case QImage::Format_ARGB32_Premultiplied:
    case QImage::Format_RGB32: {
        cv::Mat mat(inImage.height(), inImage.width(), CV_8UC4,
                    const_cast<uchar*>(inImage.bits()), inImage.bytesPerLine());
        return clone ? mat.clone() : mat;
    }
    case QImage::Format_RGBA8888:
    case QImage::Format_RGBA8888_Premultiplied: {
        cv::Mat mat(inImage.height(), inImage.width(), CV_8UC4,
                    const_cast<uchar*>(inImage.bits()), inImage.bytesPerLine());
        return clone ? mat.clone() : mat;
    }
    case QImage::Format_RGB888: {
        cv::Mat mat(inImage.height(), inImage.width(), CV_8UC3,
                    const_cast<uchar*>(inImage.bits()), inImage.bytesPerLine());
        return clone ? mat.clone() : mat;
    }
    default:
        break;
    }
    return cv::Mat();
}

inline cv::Mat QImageToBgr(const QImage& image)
{
    QImage src = image;
    if (src.format() != QImage::Format_RGB888 &&
        src.format() != QImage::Format_RGB32 &&
        src.format() != QImage::Format_ARGB32 &&
        src.format() != QImage::Format_ARGB32_Premultiplied &&
        src.format() != QImage::Format_RGBA8888) {
        src = src.convertToFormat(QImage::Format_ARGB32);
    }

    cv::Mat mat = QImageToMat(src, true);
    if (mat.empty()) {
        src = image.convertToFormat(QImage::Format_ARGB32);
        mat = QImageToMat(src, true);
    }
    if (mat.empty()) {
        return {};
    }

    cv::Mat bgr;
    if (mat.type() == CV_8UC4) {
        if (src.format() == QImage::Format_RGBA8888 ||
            src.format() == QImage::Format_RGBA8888_Premultiplied) {
            cv::cvtColor(mat, bgr, cv::COLOR_RGBA2BGR);
        } else {
            cv::cvtColor(mat, bgr, cv::COLOR_BGRA2BGR);
        }
    } else if (mat.type() == CV_8UC3) {
        cv::cvtColor(mat, bgr, cv::COLOR_RGB2BGR);
    } else {
        return {};
    }
    return bgr;
}

inline QImage MatToQImage(const cv::Mat& mat)
{
    if (mat.empty()) {
        return {};
    }

    if (mat.type() == CV_8UC4) {
        QImage image(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step), QImage::Format_ARGB32);
        return image.copy();
    }
    if (mat.type() == CV_8UC3) {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        QImage image(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888);
        return image.copy();
    }
    if (mat.type() == CV_8UC1) {
        QImage image(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step), QImage::Format_Grayscale8);
        return image.copy();
    }
    return {};
}

inline QImage BgrToQImage(const cv::Mat& bgr)
{
    if (bgr.empty()) {
        return {};
    }
    if (bgr.type() == CV_8UC4) {
        cv::Mat bgra = bgr;
        QImage image(bgra.data, bgra.cols, bgra.rows, static_cast<int>(bgra.step), QImage::Format_ARGB32);
        return image.copy();
    }
    return MatToQImage(bgr);
}

inline QImage BgraToQImage(const cv::Mat& bgra)
{
    if (bgra.empty() || bgra.type() != CV_8UC4) {
        return {};
    }
    QImage image(bgra.data, bgra.cols, bgra.rows, static_cast<int>(bgra.step), QImage::Format_ARGB32);
    return image.copy();
}

} // namespace CvBridge
