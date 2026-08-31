#include "CodeScanner.h"
#include "CvBridge.h"

#include <opencv2/objdetect.hpp>
#if __has_include(<opencv2/objdetect/barcode.hpp>) && defined(NT_HAS_OPENCV_BARCODE)
#include <opencv2/objdetect/barcode.hpp>
#endif

#include <QRect>

namespace {

QRect pointsToRect(const std::vector<cv::Point>& points)
{
    if (points.empty()) {
        return {};
    }
    cv::Rect r = cv::boundingRect(points);
    return QRect(r.x, r.y, r.width, r.height);
}

QRect matPointsToRect(const cv::Mat& points)
{
    if (points.empty()) {
        return {};
    }
    std::vector<cv::Point> pts;
    if (points.total() >= 4) {
        for (int i = 0; i < static_cast<int>(points.total()) && i < 4; ++i) {
            cv::Point2f p = points.at<cv::Point2f>(i);
            pts.emplace_back(cv::Point(cvRound(p.x), cvRound(p.y)));
        }
    }
    return pointsToRect(pts);
}

void appendUnique(QVector<CodeScanResult>& out, const CodeScanResult& item)
{
    if (item.text.isEmpty()) {
        return;
    }
    for (const auto& existing : out) {
        if (existing.text == item.text) {
            return;
        }
    }
    out.push_back(item);
}

} // namespace

namespace CodeScanner {

QVector<CodeScanResult> scan(const QImage& image)
{
    QVector<CodeScanResult> results;
    if (image.isNull()) {
        return results;
    }

    cv::Mat bgr = CvBridge::QImageToBgr(image);
    if (bgr.empty()) {
        return results;
    }

    try {
        cv::QRCodeDetector qr;
        std::vector<std::string> decoded;
        cv::Mat points;
        if (qr.detectAndDecodeMulti(bgr, decoded, points)) {
            for (size_t i = 0; i < decoded.size(); ++i) {
                CodeScanResult item;
                item.text = QString::fromStdString(decoded[i]);
                item.type = QStringLiteral("QR");
                if (!points.empty() && points.rows > static_cast<int>(i)) {
                    item.boundingRect = matPointsToRect(points.row(static_cast<int>(i)));
                }
                appendUnique(results, item);
            }
        } else {
            std::string single = qr.detectAndDecode(bgr, points);
            if (!single.empty()) {
                CodeScanResult item;
                item.text = QString::fromStdString(single);
                item.type = QStringLiteral("QR");
                item.boundingRect = matPointsToRect(points);
                appendUnique(results, item);
            }
        }
    } catch (...) {
        // Keep scanning other detectors.
    }

#if __has_include(<opencv2/objdetect/barcode.hpp>) && defined(NT_HAS_OPENCV_BARCODE)
    try {
        cv::barcode::BarcodeDetector barcode;
        std::vector<std::string> decodedInfo;
        std::vector<std::string> decodedType;
        cv::Mat points;
        if (barcode.detectAndDecodeWithType(bgr, decodedInfo, decodedType, points)) {
            for (size_t i = 0; i < decodedInfo.size(); ++i) {
                CodeScanResult item;
                item.text = QString::fromStdString(decodedInfo[i]);
                item.type = (i < decodedType.size() && !decodedType[i].empty())
                    ? QString::fromStdString(decodedType[i])
                    : QStringLiteral("Barcode");
                if (!points.empty() && points.rows > static_cast<int>(i)) {
                    item.boundingRect = matPointsToRect(points.row(static_cast<int>(i)));
                }
                appendUnique(results, item);
            }
        }
    } catch (...) {
        // Barcode detector may be unavailable in some builds.
    }
#endif

    return results;
}

QString scanToText(const QImage& image)
{
    const QVector<CodeScanResult> results = scan(image);
    QStringList parts;
    for (const auto& item : results) {
        parts.push_back(item.text);
    }
    return parts.join(QLatin1Char('\n'));
}

} // namespace CodeScanner
