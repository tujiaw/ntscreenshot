#include "SmartMask.h"
#include "CvBridge.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QIODevice>

namespace SmartMask {

namespace {

cv::CascadeClassifier& faceCascade()
{
    static cv::CascadeClassifier cascade;
    static bool tried = false;
    if (tried) {
        return cascade;
    }
    tried = true;

    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/haarcascade_frontalface_default.xml"),
        QStringLiteral(":/opencv/haarcascade_frontalface_default.xml"),
        QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral("../data/haarcascade_frontalface_default.xml")),
    };

    for (const QString& path : candidates) {
        if (path.startsWith(QLatin1Char(':'))) {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) {
                continue;
            }
            const QByteArray data = file.readAll();
            const QString tmp = QDir::temp().filePath(QStringLiteral("nt_haarcascade_face.xml"));
            QFile out(tmp);
            if (out.open(QIODevice::WriteOnly)) {
                out.write(data);
                out.close();
                if (cascade.load(tmp.toStdString())) {
                    return cascade;
                }
            }
        } else if (QFile::exists(path) && cascade.load(path.toStdString())) {
            return cascade;
        }
    }
    return cascade;
}

QVector<QRect> detectFaces(const cv::Mat& bgr)
{
    QVector<QRect> rects;
    cv::CascadeClassifier& cascade = faceCascade();
    if (cascade.empty()) {
        return rects;
    }

    cv::Mat gray;
    cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
    cv::equalizeHist(gray, gray);

    std::vector<cv::Rect> faces;
    cascade.detectMultiScale(gray, faces, 1.1, 3, 0, cv::Size(24, 24));
    for (const cv::Rect& r : faces) {
        rects.push_back(QRect(r.x, r.y, r.width, r.height));
    }
    return rects;
}

QVector<QRect> detectTextBlocks(const cv::Mat& bgr)
{
    QVector<QRect> rects;
    cv::Mat gray;
    cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);

    cv::Mat grad;
    cv::Mat sobelX;
    cv::Sobel(gray, sobelX, CV_16S, 1, 0, 3);
    cv::convertScaleAbs(sobelX, grad);

    cv::Mat binary;
    cv::threshold(grad, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(15, 3));
    cv::Mat closed;
    cv::morphologyEx(binary, closed, cv::MORPH_CLOSE, kernel);
    cv::dilate(closed, closed, cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3)));

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(closed, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    const double imgArea = static_cast<double>(bgr.cols) * bgr.rows;
    for (const auto& contour : contours) {
        cv::Rect r = cv::boundingRect(contour);
        if (r.width < 24 || r.height < 10) {
            continue;
        }
        if (r.height > bgr.rows / 3) {
            continue;
        }
        const double area = r.area();
        if (area < 400 || area > imgArea * 0.35) {
            continue;
        }
        const double aspect = static_cast<double>(r.width) / std::max(1, r.height);
        if (aspect < 1.2) {
            continue;
        }
        rects.push_back(QRect(r.x, r.y, r.width, r.height));
    }
    return rects;
}

void mosaicRegion(cv::Mat& bgr, const QRect& region, int blockSize)
{
    QRect r = region.intersected(QRect(0, 0, bgr.cols, bgr.rows));
    if (r.width() < 2 || r.height() < 2) {
        return;
    }

    const int block = std::max(4, blockSize);
    cv::Rect roi(r.x(), r.y(), r.width(), r.height());
    cv::Mat patch = bgr(roi);
    cv::Mat small;
    cv::resize(patch, small,
               cv::Size(std::max(1, patch.cols / block), std::max(1, patch.rows / block)),
               0, 0, cv::INTER_AREA);
    cv::Mat mosaic;
    cv::resize(small, mosaic, patch.size(), 0, 0, cv::INTER_NEAREST);
    cv::Mat blurred;
    cv::GaussianBlur(patch, blurred, cv::Size(0, 0), 4.0);
    cv::addWeighted(mosaic, 0.75, blurred, 0.25, 0, patch);
}

} // namespace

QVector<QRect> detectRegions(const QImage& image, Target target)
{
    QVector<QRect> rects;
    if (image.isNull()) {
        return rects;
    }
    cv::Mat bgr = CvBridge::QImageToBgr(image);
    if (bgr.empty()) {
        return rects;
    }

    const int flags = static_cast<int>(target);
    if (flags & static_cast<int>(Target::Faces)) {
        rects += detectFaces(bgr);
    }
    if (flags & static_cast<int>(Target::TextBlocks)) {
        rects += detectTextBlocks(bgr);
    }
    return rects;
}

QImage applyMosaic(const QImage& image, const QVector<QRect>& regions, int blockSize)
{
    if (image.isNull() || regions.isEmpty()) {
        return image;
    }
    cv::Mat bgr = CvBridge::QImageToBgr(image);
    if (bgr.empty()) {
        return image;
    }
    for (const QRect& region : regions) {
        mosaicRegion(bgr, region, blockSize);
    }
    return CvBridge::BgrToQImage(bgr);
}

QImage autoMask(const QImage& image, Target target, int blockSize)
{
    return applyMosaic(image, detectRegions(image, target), blockSize);
}

} // namespace SmartMask
