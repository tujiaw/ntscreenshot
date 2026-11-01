#include "QrCodeGenerator.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QImageWriter>
#include <QPainter>
#include <QSaveFile>
#include <QUuid>
#include <opencv2/objdetect.hpp>
#include <stdexcept>

namespace QrCodeGenerator {
QImage create(const QString& text, QString* error)
{
    if (error) error->clear();
    if (text.isEmpty()) {
        if (error) *error = QCoreApplication::translate("App", "没有可生成二维码的文字");
        return {};
    }
    // Leave space for the UTF-8 ECI header at medium error correction.
    if (text.size() > 2300) {
        if (error) *error = QCoreApplication::translate("App", "文字过长，请缩短后再生成二维码");
        return {};
    }
    const QByteArray utf8 = text.toUtf8();
    if (utf8.size() > 2300) {
        if (error) *error = QCoreApplication::translate("App", "文字过长，请缩短后再生成二维码");
        return {};
    }
    try {
        cv::QRCodeEncoder::Params parameters;
        parameters.mode = cv::QRCodeEncoder::MODE_ECI;
        parameters.correction_level = cv::QRCodeEncoder::CORRECT_LEVEL_M;
        cv::Mat modules;
        cv::QRCodeEncoder::create(parameters)->encode(std::string(utf8.constData(), utf8.size()), modules);
        if (modules.empty() || modules.type() != CV_8UC1) throw std::runtime_error("Invalid QR matrix");
        constexpr int quietZone = 4, scale = 8;
        QImage image((modules.cols + quietZone * 2) * scale,
                     (modules.rows + quietZone * 2) * scale, QImage::Format_RGB32);
        if (image.isNull()) throw std::runtime_error("QR image allocation failed");
        image.fill(Qt::white);
        QPainter painter(&image);
        for (int y = 0; y < modules.rows; ++y)
            for (int x = 0; x < modules.cols; ++x)
                if (modules.at<uchar>(y, x) == 0)
                    painter.fillRect((x + quietZone) * scale, (y + quietZone) * scale, scale, scale, Qt::black);
        return image;
    } catch (const std::exception&) {
        if (error) *error = QCoreApplication::translate("App", "二维码生成失败，请尝试缩短文字");
        return {};
    }
}

QString save(const QString& text, const QString& directory, QString* error)
{
    if (error) error->clear();
    const QImage image = create(text, error);
    if (image.isNull()) return {};
    if (directory.trimmed().isEmpty() || !QDir().mkpath(directory)) {
        if (error) *error = QCoreApplication::translate("App", "无法创建保存目录");
        return {};
    }
    const QString name = QStringLiteral("QRCode_%1_%2.jpg")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz")),
             QUuid::createUuid().toString(QUuid::Id128).left(8));
    const QString path = QDir(directory).absoluteFilePath(name);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = QCoreApplication::translate("App", "二维码保存失败：%1").arg(file.errorString());
        return {};
    }
    QImageWriter writer(&file, "jpg");
    writer.setQuality(100);
    if (!writer.write(image)) {
        if (error) *error = QCoreApplication::translate("App", "二维码保存失败：%1").arg(writer.errorString());
        file.cancelWriting();
        return {};
    }
    if (!file.commit()) {
        if (error) *error = QCoreApplication::translate("App", "二维码保存失败：%1").arg(file.errorString());
        return {};
    }
    return path;
}
}
