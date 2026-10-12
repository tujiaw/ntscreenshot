#include "ImageDocument.h"
#include "ImageMosaic.h"
#include "ImageAnnotation.h"
#include "ImageCvOperations.h"

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
#include <QPdfWriter>
#include <QPageSize>
#include <QPageLayout>
#include <QSaveFile>
#include <QPainterPath>
#include <cmath>
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
        if (pixels(original.size()) > MaxEditablePixels) {
            result.error = message("图片尺寸超过 7200 万像素的编辑上限。"); return result;
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
            } else if (operation.kind >= EditOperation::Annotate) {
                image = image.convertToFormat(QImage::Format_ARGB32);
                if (image.isNull()) throw std::bad_alloc();
                const QRect region = operation.rect.intersected(image.rect());
                if (operation.kind == EditOperation::Cutout || (operation.kind == EditOperation::Filter && operation.variant >= 3)) {
                    auto processed = applyCvOperation(image, operation);
                    if (!processed.error.isEmpty()) return processed;
                    image = processed.image;
                } else if (operation.kind == EditOperation::Mosaic) {
                    if ((!operation.directDrawing && region.isEmpty()) || (operation.directDrawing && operation.brushPoints.isEmpty())) {
                        result.error = message("裁剪区域无效。"); return result;
                    }
                    const QImage source = image;
                    QPainter painter(&image);
                    if (!painter.isActive()) throw std::bad_alloc();
                    paintMosaic(painter, source, operation);
                } else if (operation.kind == EditOperation::Perspective) {
                    if (operation.corners.size() != 4) { result.error = message("裁剪区域无效。"); return result; }
                    std::vector<cv::Point2f> from;
                    for (const auto& p : operation.corners) from.emplace_back(float(p.x()), float(p.y()));
                    const QSize size = operation.size;
                    if (!size.isValid() || pixels(size) > MaxEditablePixels ||
                        !cv::isContourConvex(from) || std::abs(cv::contourArea(from)) < 1) {
                        result.error = message("裁剪区域无效。"); return result;
                    }
                    const std::vector<cv::Point2f> to = {{0, 0}, {float(size.width()-1), 0},
                        {float(size.width()-1), float(size.height()-1)}, {0, float(size.height()-1)}};
                    cv::Mat output;
                    const QImage premultiplied = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
                    cv::warpPerspective(CvBridge::QImageToMat(premultiplied), output,
                        cv::getPerspectiveTransform(from, to), cv::Size(size.width(), size.height()),
                        cv::INTER_LINEAR, cv::BORDER_CONSTANT);
                    image = QImage(output.data, output.cols, output.rows, int(output.step),
                        QImage::Format_ARGB32_Premultiplied).convertToFormat(QImage::Format_ARGB32);
                } else if (operation.kind == EditOperation::Filter || operation.kind == EditOperation::AdjustColor) {
                    const double sat = 1 + qBound(-100, operation.saturation, 100) / 100.;
                    const double contrast = 1 + qBound(-100, operation.contrast, 100) / 100.;
                    const double brightness = qBound(-100, operation.brightness, 100)*2.55;
                    const double temperature = qBound(-100, operation.temperature, 100)*.6;
                    const double tint = qBound(-100, operation.tint, 100)*.6;
                    for (int y = 0; y < image.height(); ++y) {
                        auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
                        for (int x = 0; x < image.width(); ++x) {
                            const QRgb pixel = line[x];
                            int r = qRed(pixel), g = qGreen(pixel), b = qBlue(pixel);
                            if (operation.kind == EditOperation::Filter) {
                                if (operation.variant == 0) r = g = b = qGray(pixel);
                                else if (operation.variant == 1) {
                                    r = qMin(255, qRound(.393*qRed(pixel) + .769*qGreen(pixel) + .189*qBlue(pixel)));
                                    g = qMin(255, qRound(.349*qRed(pixel) + .686*qGreen(pixel) + .168*qBlue(pixel)));
                                    b = qMin(255, qRound(.272*qRed(pixel) + .534*qGreen(pixel) + .131*qBlue(pixel)));
                                } else { r = 255-r; g = 255-g; b = 255-b; }
                            } else {
                                const double gray = .299*r + .587*g + .114*b;
                                auto adjust = [&](int channel, double offset) { return qBound(0, qRound(((gray + (channel-gray)*sat)-127.5)*contrast + 127.5 + brightness + offset), 255); };
                                r = adjust(r, temperature + tint/2); g = adjust(g, -tint/2); b = adjust(b, -temperature + tint/2);
                            }
                            line[x] = qRgba(r, g, b, operation.kind == EditOperation::AdjustColor ? qRound(qAlpha(pixel) * qBound(0, operation.opacity, 100)/100.) : qAlpha(pixel));
                        }
                    }
                } else if (operation.kind == EditOperation::Annotate) {
                    QPainter painter(&image);
                    painter.setClipRect(image.rect());
                    paintAnnotation(painter, operation);
                } else if (operation.kind == EditOperation::Watermark) {
                    QPainter painter(&image);
                    painter.setClipRect(image.rect());
                    painter.setRenderHint(QPainter::Antialiasing);
                    painter.setPen(operation.color);
                    painter.setOpacity(qBound(0, operation.opacity, 100)/100.);
                    QFont font = painter.font(); font.setPixelSize(qBound(8, operation.strength, 200)); painter.setFont(font);
                    if (operation.variant == 0) {
                        if (region.isEmpty()) { result.error = message("裁剪区域无效。"); return result; }
                        painter.drawText(region, Qt::AlignCenter | Qt::TextWordWrap, operation.text);
                    } else {
                        const int margin = qBound(0, operation.margin, qMin(image.width(), image.height())/2);
                        const QRect area = image.rect().adjusted(margin, margin, -margin, -margin);
                        if (operation.variant == 6) {
                            const QFontMetrics metrics(font);
                            const int stepX = qMax(32, metrics.horizontalAdvance(operation.text) + 64);
                            const int stepY = qMax(32, metrics.height() + 64);
                            for (int y = margin; y < image.height(); y += stepY)
                                for (int x = margin; x < image.width(); x += stepX)
                                    painter.drawText(QPoint(x, y + metrics.ascent()), operation.text);
                        } else {
                            const int alignments[] = {0, Qt::AlignRight | Qt::AlignBottom, Qt::AlignLeft | Qt::AlignBottom,
                                Qt::AlignRight | Qt::AlignTop, Qt::AlignLeft | Qt::AlignTop, Qt::AlignCenter};
                            painter.drawText(area, alignments[qBound(1, operation.variant, 5)] | Qt::TextWordWrap, operation.text);
                        }
                    }
                } else {
                    if (region.isEmpty()) { result.error = message("裁剪区域无效。"); return result; }
                    QPainter painter(&image); painter.setClipRect(region);
                    painter.drawImage(region, operation.asset);
                }
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
                        result.error = message("图片尺寸超过 7200 万像素的编辑上限。");
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
            result.error = message("图片尺寸超过 7200 万像素的编辑上限。");
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
                             const QDateTime& expectedModified, qint64 expectedSize,
                             const ImageConversionOptions& options) {
    SaveResult result;
    try {
        if (image.isNull()) { result.error = message("没有可保存的图片。"); return result; }
        if (format != "png" && format != "jpeg" && format != "jpg" && format != "bmp" && format != "webp" && format != "pdf" && format != "ico") {
            result.error = message("图片保存失败或内存不足。");
            return result;
        }
        QImage output = image;
        if (format == "ico") {
            if (options.icoSize < 1 || options.icoSize > 256) {
                result.error = message("ICO 图标尺寸必须在 1～256 像素之间。"); return result;
            }
            const QImage scaled = image.scaled(options.icoSize, options.icoSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            if (scaled.isNull()) throw std::bad_alloc();
            output = QImage(options.icoSize, options.icoSize, QImage::Format_ARGB32);
            if (output.isNull()) throw std::bad_alloc();
            output.fill(Qt::transparent);
            QPainter painter(&output);
            painter.drawImage((output.width()-scaled.width())/2, (output.height()-scaled.height())/2, scaled);
        } else if (options.outputSize != QSize()) {
            if (options.outputSize.isEmpty() || pixels(options.outputSize) > MaxEditablePixels) {
                result.error = message("输出尺寸不能超过 7200 万像素。"); return result;
            }
            output = image.scaled(options.outputSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            if (output.isNull()) throw std::bad_alloc();
        }
        if (options.grayscale) {
            output = output.convertToFormat(QImage::Format_ARGB32);
            if (output.isNull()) throw std::bad_alloc();
            for (int y = 0; y < output.height(); ++y) {
                auto* row = reinterpret_cast<QRgb*>(output.scanLine(y));
                for (int x = 0; x < output.width(); ++x) {
                    const int gray = qGray(row[x]);
                    row[x] = qRgba(gray, gray, gray, qAlpha(row[x]));
                }
            }
        }
        if (format == "jpeg" || format == "jpg" || format == "bmp") {
            const QImage transparent = output;
            output = QImage(transparent.size(), QImage::Format_RGB32);
            if (output.isNull()) throw std::bad_alloc();
            output.fill(Qt::white);
            QPainter painter(&output);
            painter.drawImage(0, 0, transparent);
        }
        QSaveFile file(path);
        file.setDirectWriteFallback(false);
        if (!file.open(QIODevice::WriteOnly)) { result.error = file.errorString(); return result; }
        if (format == "pdf") {
            QPdfWriter writer(&file);
            writer.setResolution(96);
            writer.setCreator(QStringLiteral("ntscreenshot"));
            writer.setTitle(QFileInfo(path).completeBaseName());
            QPageSize paper(QPageSize::A4);
            auto orientation = QPageLayout::Portrait;
            QMarginsF margins;
            if (options.pdfA4) {
                if (options.pdfMarginMm < 0 || options.pdfMarginMm > 50) {
                    result.error = message("PDF 页面设置无效。"); return result;
                }
                const int margin = options.pdfMarginMm;
                margins = QMarginsF(margin, margin, margin, margin);
                if (options.pdfOrientation == 2 || (options.pdfOrientation == 0 && output.width() > output.height()))
                    orientation = QPageLayout::Landscape;
            } else {
                // Keep long screenshots within the standard PDF page-size limit.
                const double scale = qMin(72.0 / 96.0, 14400.0 / qMax(output.width(), output.height()));
                paper = QPageSize(QSizeF(qMax(1.0, output.width() * scale), qMax(1.0, output.height() * scale)),
                    QPageSize::Point, QString(), QPageSize::ExactMatch);
            }
            if (!writer.setPageLayout(QPageLayout(paper, orientation, margins, QPageLayout::Millimeter))) {
                result.error = message("PDF 页面设置无效。"); return result;
            }
            QPainter painter;
            if (!painter.begin(&writer)) { result.error = message("PDF 保存失败。"); return result; }
            const QRectF page(0, 0, writer.width(), writer.height());
            const QSizeF size = QSizeF(output.size()).scaled(page.size(), Qt::KeepAspectRatio);
            const QRectF target(QPointF((page.width() - size.width()) / 2, (page.height() - size.height()) / 2), size);
            painter.fillRect(page, Qt::white);
            painter.drawImage(target, output);
            if (!painter.end()) { result.error = message("PDF 保存失败。"); return result; }
        } else {
            QImageWriter writer(&file, format);
            if (format == "jpeg" || format == "jpg" || format == "webp") writer.setQuality(qBound(1, options.quality, 100));
            if (format == "png") writer.setCompression(qBound(0, options.pngCompression, 100));
            if (!writer.write(output)) { result.error = writer.errorString(); return result; }
        }
        if (expectedModified.isValid()) {
            const QFileInfo source(path);
            if (!source.exists() || source.lastModified() != expectedModified || source.size() != expectedSize) {
                result.error = QCoreApplication::translate("ImageBrowser::ImageBrowserWindow", "原文件已被外部修改，请将编辑结果另存为新文件。");
                return result;
            }
        }
        result.success = file.commit();
        if (result.success && format != "pdf") result.image = output;
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
