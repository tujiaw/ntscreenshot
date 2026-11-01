#include "core/imaging/CodeScanner.h"
#include "core/imaging/QrCodeGenerator.h"

#include <QCoreApplication>
#include <QDebug>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
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
                QTemporaryDir output;
                if (!output.isValid()) return 2;
                const QString payload = QStringLiteral("https://example.com/?q=二维码&x=1\n用户名：demo\n密码：123456 😀");
                QString error;
                const QString directory = output.filePath(QStringLiteral("二维码图片"));
                {
                    const QString path = QrCodeGenerator::save(payload, directory, &error);
                    if (path.isEmpty()) { qCritical() << error; return 4; }
                    if (QFileInfo(path).absolutePath() != QDir(directory).absolutePath() || QFileInfo(path).suffix() != QStringLiteral("jpg")) return 5;
                    const auto decoded = CodeScanner::scan(QImage(path));
                    bool exact = false;
                    for (const auto& item : decoded) if (item.text == payload) exact = true;
                    if (!exact) { qCritical() << "JPEG QR roundtrip failed"; return 6; }
                    const QString second = QrCodeGenerator::save(payload, directory, &error);
                    if (second.isEmpty() || second == path || !QFileInfo::exists(path)) return 7;
                }
                if (!QrCodeGenerator::create({}, &error).isNull() || error.isEmpty()) return 8;
                if (!QrCodeGenerator::create(QString(2301, u'x'), &error).isNull() || error.isEmpty()) return 9;
                const QString blocked = output.filePath(QStringLiteral("not-a-directory"));
                QFile occupied(blocked);
                if (!occupied.open(QIODevice::WriteOnly) || occupied.write("preserve") != 8) return 11;
                occupied.close();
                if (!QrCodeGenerator::save(payload, blocked, &error).isEmpty()) return 12;
                if (!occupied.open(QIODevice::ReadOnly) || occupied.readAll() != QByteArray("preserve")) return 13;
                return 0;
            }
        }
        qCritical() << "QR decoding failed; check that OpenCV includes quirc.";
    } catch (const cv::Exception& error) {
        qCritical() << error.what();
    }
    return 1;
}
