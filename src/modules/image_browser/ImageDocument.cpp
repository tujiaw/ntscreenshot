#include "ImageDocument.h"

#include "core/imaging/CvBridge.h"
#include <QCoreApplication>
#include <QColorSpace>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QImageIOHandler>
#include <QImageWriter>
#include <QMovie>
#include <QPainter>
#include <QSaveFile>
#include <new>

namespace ImageBrowser {
namespace {
QString message(const char* text) { return QCoreApplication::translate("ImageBrowser", text); }
qint64 pixels(QSize size) { return qint64(size.width()) * size.height(); }
}

void ImageDocument::load(const QString& file, const ImageResult& result) {
    path = file;
    original = current = result.image;
    operations.clear();
    position = 0;
    frozenFrame = false;
    limited = result.limited;
    modified = result.modified;
    fileSize = result.fileSize;
}

void ImageDocument::append(const EditOperation& operation) {
    operations.resize(position);
    operations.append(operation);
    ++position;
}

ImageResult ImageDocument::read(const QString& path, const QSize& thumbnail) {
    ImageResult result;
    try {
        const QFileInfo info(path);
        result.modified = info.lastModified();
        result.fileSize = info.size();
        QImageReader reader(path);
        reader.setAutoTransform(true);
        const QSize rawSize = reader.size();
        result.originalSize = rawSize;
        if (reader.transformation() & QImageIOHandler::TransformationRotate90) result.originalSize.transpose();
        const auto format = reader.format().toLower();
        result.animationSupported = reader.supportsAnimation() && QMovie::supportedFormats().contains(format);
        result.animated = reader.supportsAnimation() && reader.imageCount() != 1;
        result.limited = pixels(rawSize) > MaxEditablePixels;
        if (thumbnail.isValid() || result.limited) {
            const QSize bound = thumbnail.isValid() ? thumbnail : QSize(2048, 2048);
            if (rawSize.isValid()) reader.setScaledSize(rawSize.scaled(bound, Qt::KeepAspectRatio).expandedTo(QSize(1, 1)));
        }
        result.image = reader.read();
        if (result.image.isNull()) result.error = reader.errorString();
        else if (!thumbnail.isValid() && !result.limited) result.originalSize = result.image.size();
        if (!thumbnail.isValid() && pixels(result.image.size()) > MaxEditablePixels) {
            result.image = result.image.scaled(2048, 2048, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            result.limited = true;
        }
    } catch (const std::exception&) {
        result.error = message("图片解码失败或内存不足。");
    }
    return result;
}

ImageResult ImageDocument::render(const QImage& original,
                                 const QVector<EditOperation>& operations, int count) {
    ImageResult result;
    try {
        if (original.isNull() || count < 0 || count > operations.size()) {
            result.error = message("裁剪区域无效。");
            return result;
        }
        QImage image = original;
        for (int i = 0; i < count; ++i) {
            const auto& operation = operations[i];
            if (operation.kind == EditOperation::Crop) {
                if (operation.rect.isEmpty() || !image.rect().contains(operation.rect)) {
                    result.error = message("裁剪区域无效。");
                    return result;
                }
                image = image.copy(operation.rect);
            } else {
                // BGRA stays BGRA through geometric transforms; alpha is not discarded.
                const QImage argb = image.convertToFormat(QImage::Format_ARGB32);
                if (argb.isNull()) throw std::bad_alloc();
                const cv::Mat input = CvBridge::QImageToMat(argb);
                cv::Mat output;
                switch (operation.kind) {
                case EditOperation::RotateLeft: cv::rotate(input, output, cv::ROTATE_90_COUNTERCLOCKWISE); break;
                case EditOperation::RotateRight: cv::rotate(input, output, cv::ROTATE_90_CLOCKWISE); break;
                case EditOperation::FlipHorizontal: cv::flip(input, output, 1); break;
                case EditOperation::FlipVertical: cv::flip(input, output, 0); break;
                case EditOperation::Resize:
                    if (!operation.size.isValid() || pixels(operation.size) > MaxEditablePixels) {
                        result.error = message("图片尺寸超过 4000 万像素的编辑上限。");
                        return result;
                    }
                    // Premultiplication avoids dark/colored fringes around transparent pixels.
                    {
                        const QImage premultiplied = argb.convertToFormat(QImage::Format_ARGB32_Premultiplied);
                        cv::resize(CvBridge::QImageToMat(premultiplied), output,
                                   cv::Size(operation.size.width(), operation.size.height()), 0, 0,
                                   pixels(operation.size) < pixels(image.size()) ? cv::INTER_AREA : cv::INTER_CUBIC);
                        QImage resized(output.data, output.cols, output.rows, int(output.step),
                                       QImage::Format_ARGB32_Premultiplied);
                        image = resized.convertToFormat(QImage::Format_ARGB32);
                    }
                    break;
                default: break;
                }
                if (operation.kind != EditOperation::Resize) image = CvBridge::MatToQImage(output);
            }
            if (image.isNull()) throw std::bad_alloc();
        }
        image.setColorSpace(original.colorSpace());
        result.image = image;
        result.originalSize = image.size();
    } catch (const std::exception&) {
        result.error = message("图片处理失败或内存不足。");
    }
    return result;
}

ImageResult ImageDocument::readFrame(const QString& path, int frame) {
    ImageResult result;
    try {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        if (pixels(reader.size()) > MaxEditablePixels) {
            result.error = message("图片尺寸超过 4000 万像素的编辑上限。");
            return result;
        }
        if (reader.jumpToImage(frame)) result.image = reader.read();
        else {
            for (int i = 0; i <= frame; ++i) {
                result.image = reader.read();
                if (result.image.isNull()) break;
            }
        }
        if (result.image.isNull()) result.error = reader.errorString();
        result.originalSize = result.image.size();
    } catch (const std::exception&) {
        result.error = message("图片解码失败或内存不足。");
    }
    return result;
}

SaveResult ImageDocument::save(const QString& path, const QImage& image, const QByteArray& format,
                             const QDateTime& expectedModified, qint64 expectedSize) {
    SaveResult result;
    try {
        if (image.isNull()) { result.error = message("没有可保存的图片。"); return result; }
        if (format != "png" && format != "jpeg" && format != "jpg" && format != "bmp" && format != "webp") {
            result.error = message("图片保存失败或内存不足。");
            return result;
        }
        QImage output = image;
        if (format == "jpeg" || format == "jpg" || format == "bmp") {
            output = QImage(image.size(), QImage::Format_RGB32);
            if (output.isNull()) throw std::bad_alloc();
            output.fill(Qt::white);
            QPainter painter(&output);
            painter.drawImage(0, 0, image);
        }
        QSaveFile file(path);
        file.setDirectWriteFallback(false);
        if (!file.open(QIODevice::WriteOnly)) { result.error = file.errorString(); return result; }
        QImageWriter writer(&file, format);
        if (format == "jpeg" || format == "jpg" || format == "webp") writer.setQuality(95);
        if (!writer.write(output)) { result.error = writer.errorString(); return result; }
        if (expectedModified.isValid()) {
            const QFileInfo source(path);
            if (!source.exists() || source.lastModified() != expectedModified || source.size() != expectedSize) {
                result.error = QCoreApplication::translate("ImageBrowser::ImageBrowserWindow", "原文件已被外部修改，请将编辑结果另存为新文件。");
                return result;
            }
        }
        result.success = file.commit();
        if (!result.success) result.error = file.errorString();
    } catch (const std::exception&) {
        result.error = message("图片保存失败或内存不足。");
    }
    return result;
}

QString ImageDocument::suggestedPath(const QString& source, const QByteArray& format) {
    const QFileInfo info(source);
    const QString stem = info.completeBaseName() + QStringLiteral("-edited");
    const QString extension = QString::fromLatin1(format == "jpeg" ? QByteArray("jpg") : format);
    QString path = info.dir().filePath(stem + QStringLiteral(".") + extension);
    for (int i = 2; QFileInfo::exists(path); ++i)
        path = info.dir().filePath(stem + QStringLiteral("-%1.").arg(i) + extension);
    return path;
}

bool ImageDocument::sourceUnchanged() const {
    const QFileInfo info(path);
    return info.exists() && info.lastModified() == modified && info.size() == fileSize;
}
} // namespace ImageBrowser
