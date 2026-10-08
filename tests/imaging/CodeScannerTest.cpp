#include "core/imaging/CodeScanner.h"

#include <QCoreApplication>
#include <QDebug>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    const QString text = QStringLiteral("ntscreenshot minimal OpenCV QR test");
    try {
        cv::Mat code;
        cv::QRCodeEncoder::create()->encode(text.toStdString(), code);
        cv::Mat bordered;
        cv::copyMakeBorder(code, bordered, 4, 4, 4, 4, cv::BORDER_CONSTANT, cv::Scalar(255));
        cv::Mat enlarged;
        cv::resize(bordered, enlarged, cv::Size(), 10, 10, cv::INTER_NEAREST);
        QImage image(enlarged.data, enlarged.cols, enlarged.rows,
                     static_cast<int>(enlarged.step), QImage::Format_Grayscale8);
        const auto results = CodeScanner::scan(image);
        for (const auto& result : results) {
            if (result.type == QStringLiteral("QR") && result.text == text
                && !result.boundingRect.isEmpty()) {
                return 0;
            }
        }
        qCritical() << "QR decoding failed; check that OpenCV includes quirc.";
    } catch (const cv::Exception& error) {
        qCritical() << error.what();
    }
    return 1;
}
