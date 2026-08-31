#include "GifEncoderWorker.h"

#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QHash>
#include <QVector>
#include <QtEndian>
#include <algorithm>
#include <limits>

namespace {
bool writeAll(QFile *file, const char *data, qint64 size)
{
    return file && file->write(data, size) == size;
}

bool writeByte(QFile *file, quint8 value)
{
    const char byte = static_cast<char>(value);
    return writeAll(file, &byte, 1);
}

bool writeLe16(QFile *file, quint16 value)
{
    const quint16 littleEndian = qToLittleEndian(value);
    return writeAll(file, reinterpret_cast<const char *>(&littleEndian), sizeof(littleEndian));
}

struct HistogramEntry {
    quint32 count = 0;
    quint64 red = 0;
    quint64 green = 0;
    quint64 blue = 0;
};

struct ColorSample {
    int red = 0;
    int green = 0;
    int blue = 0;
    quint32 count = 0;
};

struct ColorBox {
    QVector<int> samples;
    quint64 pixelCount = 0;
    int redMin = 0;
    int redMax = 0;
    int greenMin = 0;
    int greenMax = 0;
    int blueMin = 0;
    int blueMax = 0;
};

struct QuantizedFrame {
    QByteArray pixels;
    QByteArray palette;
    QVector<QRgb> colors;
};

void updateBox(ColorBox &box, const QVector<ColorSample> &samples)
{
    box.pixelCount = 0;
    box.redMin = box.greenMin = box.blueMin = 255;
    box.redMax = box.greenMax = box.blueMax = 0;
    for (int index : box.samples) {
        const ColorSample &sample = samples[index];
        box.pixelCount += sample.count;
        box.redMin = qMin(box.redMin, sample.red);
        box.redMax = qMax(box.redMax, sample.red);
        box.greenMin = qMin(box.greenMin, sample.green);
        box.greenMax = qMax(box.greenMax, sample.green);
        box.blueMin = qMin(box.blueMin, sample.blue);
        box.blueMax = qMax(box.blueMax, sample.blue);
    }
}

QVector<QRgb> buildAdaptivePalette(const QImage &image)
{
    constexpr int histogramSize = 32 * 32 * 32;
    QVector<HistogramEntry> histogram(histogramSize);
    for (int y = 0; y < image.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const QRgb pixel = line[x];
            const int key = ((qRed(pixel) >> 3) << 10)
                          | ((qGreen(pixel) >> 3) << 5)
                          | (qBlue(pixel) >> 3);
            HistogramEntry &entry = histogram[key];
            ++entry.count;
            entry.red += qRed(pixel);
            entry.green += qGreen(pixel);
            entry.blue += qBlue(pixel);
        }
    }

    QVector<ColorSample> samples;
    samples.reserve(histogramSize);
    for (const HistogramEntry &entry : histogram) {
        if (entry.count == 0) {
            continue;
        }
        samples.push_back({static_cast<int>(entry.red / entry.count),
                           static_cast<int>(entry.green / entry.count),
                           static_cast<int>(entry.blue / entry.count),
                           entry.count});
    }
    if (samples.isEmpty()) {
        return {qRgb(0, 0, 0)};
    }

    ColorBox initial;
    initial.samples.resize(samples.size());
    for (int i = 0; i < samples.size(); ++i) {
        initial.samples[i] = i;
    }
    updateBox(initial, samples);
    QVector<ColorBox> boxes{std::move(initial)};

    while (boxes.size() < 256) {
        int selected = -1;
        qint64 bestScore = -1;
        for (int i = 0; i < boxes.size(); ++i) {
            const ColorBox &box = boxes[i];
            if (box.samples.size() < 2) {
                continue;
            }
            const int range = std::max({box.redMax - box.redMin,
                                        box.greenMax - box.greenMin,
                                        box.blueMax - box.blueMin});
            const qint64 score = static_cast<qint64>(range + 1)
                               * static_cast<qint64>(box.pixelCount);
            if (score > bestScore) {
                bestScore = score;
                selected = i;
            }
        }
        if (selected < 0) {
            break;
        }

        ColorBox box = std::move(boxes[selected]);
        const int redRange = box.redMax - box.redMin;
        const int greenRange = box.greenMax - box.greenMin;
        const int blueRange = box.blueMax - box.blueMin;
        const int channel = redRange >= greenRange && redRange >= blueRange ? 0
                          : greenRange >= blueRange ? 1 : 2;
        std::sort(box.samples.begin(), box.samples.end(), [&](int left, int right) {
            const ColorSample &a = samples[left];
            const ColorSample &b = samples[right];
            return channel == 0 ? a.red < b.red
                 : channel == 1 ? a.green < b.green
                                : a.blue < b.blue;
        });

        const quint64 half = box.pixelCount / 2;
        quint64 accumulated = 0;
        int split = 0;
        for (int i = 0; i < box.samples.size() - 1; ++i) {
            accumulated += samples[box.samples[i]].count;
            if (accumulated >= half) {
                split = i + 1;
                break;
            }
        }
        if (split <= 0 || split >= box.samples.size()) {
            split = box.samples.size() / 2;
        }

        ColorBox left;
        ColorBox right;
        left.samples = box.samples.mid(0, split);
        right.samples = box.samples.mid(split);
        updateBox(left, samples);
        updateBox(right, samples);
        boxes[selected] = std::move(left);
        boxes.push_back(std::move(right));
    }

    QVector<QRgb> palette;
    palette.reserve(boxes.size());
    for (const ColorBox &box : boxes) {
        quint64 red = 0;
        quint64 green = 0;
        quint64 blue = 0;
        quint64 count = 0;
        for (int index : box.samples) {
            const ColorSample &sample = samples[index];
            red += static_cast<quint64>(sample.red) * sample.count;
            green += static_cast<quint64>(sample.green) * sample.count;
            blue += static_cast<quint64>(sample.blue) * sample.count;
            count += sample.count;
        }
        palette.push_back(qRgb(static_cast<int>(red / count),
                               static_cast<int>(green / count),
                               static_cast<int>(blue / count)));
    }
    return palette;
}

int nearestPaletteIndex(int red, int green, int blue, const QVector<QRgb> &palette,
                        QVector<qint16> &cache)
{
    const int cacheKey = ((red >> 3) << 10) | ((green >> 3) << 5) | (blue >> 3);
    if (cache[cacheKey] >= 0) {
        return cache[cacheKey];
    }

    int bestIndex = 0;
    int bestDistance = std::numeric_limits<int>::max();
    for (int i = 0; i < palette.size(); ++i) {
        const int redDelta = red - qRed(palette[i]);
        const int greenDelta = green - qGreen(palette[i]);
        const int blueDelta = blue - qBlue(palette[i]);
        const int distance = redDelta * redDelta * 3
                           + greenDelta * greenDelta * 6
                           + blueDelta * blueDelta;
        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = i;
        }
    }
    cache[cacheKey] = static_cast<qint16>(bestIndex);
    return bestIndex;
}

QuantizedFrame quantizeAdaptive(const QImage &image)
{
    const QVector<QRgb> colors = buildAdaptivePalette(image);
    QuantizedFrame result;
    result.colors = colors;
    result.pixels.resize(image.width() * image.height());
    result.palette.resize(256 * 3);

    const QRgb paddingColor = colors.constLast();
    for (int i = 0; i < 256; ++i) {
        const QRgb color = i < colors.size() ? colors[i] : paddingColor;
        result.palette[i * 3] = static_cast<char>(qRed(color));
        result.palette[i * 3 + 1] = static_cast<char>(qGreen(color));
        result.palette[i * 3 + 2] = static_cast<char>(qBlue(color));
    }

    QVector<qint16> nearestCache(32 * 32 * 32, static_cast<qint16>(-1));
    QVector<int> currentRed(image.width() + 2, 0);
    QVector<int> currentGreen(image.width() + 2, 0);
    QVector<int> currentBlue(image.width() + 2, 0);
    QVector<int> nextRed(image.width() + 2, 0);
    QVector<int> nextGreen(image.width() + 2, 0);
    QVector<int> nextBlue(image.width() + 2, 0);

    uchar *output = reinterpret_cast<uchar *>(result.pixels.data());
    for (int y = 0; y < image.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const int red = qBound(0, qRed(line[x]) + currentRed[x + 1] / 16, 255);
            const int green = qBound(0, qGreen(line[x]) + currentGreen[x + 1] / 16, 255);
            const int blue = qBound(0, qBlue(line[x]) + currentBlue[x + 1] / 16, 255);
            const int paletteIndex = nearestPaletteIndex(red, green, blue, colors, nearestCache);
            *output++ = static_cast<uchar>(paletteIndex);

            const QRgb mapped = colors[paletteIndex];
            const int redError = red - qRed(mapped);
            const int greenError = green - qGreen(mapped);
            const int blueError = blue - qBlue(mapped);
            // Floyd-Steinberg: spread quantization error to neighboring pixels
            // using the standard 7/16, 3/16, 5/16 and 1/16 weights.
            currentRed[x + 2] += redError * 7;
            currentGreen[x + 2] += greenError * 7;
            currentBlue[x + 2] += blueError * 7;
            nextRed[x] += redError * 3;
            nextGreen[x] += greenError * 3;
            nextBlue[x] += blueError * 3;
            nextRed[x + 1] += redError * 5;
            nextGreen[x + 1] += greenError * 5;
            nextBlue[x + 1] += blueError * 5;
            nextRed[x + 2] += redError;
            nextGreen[x + 2] += greenError;
            nextBlue[x + 2] += blueError;
        }
        currentRed.swap(nextRed);
        currentGreen.swap(nextGreen);
        currentBlue.swap(nextBlue);
        nextRed.fill(0);
        nextGreen.fill(0);
        nextBlue.fill(0);
    }
    return result;
}

class LzwBitWriter
{
public:
    void write9(int code)
    {
        bits_ |= static_cast<quint32>(code & 0x1ff) << bitCount_;
        bitCount_ += 9;
        while (bitCount_ >= 8) {
            bytes_.append(static_cast<char>(bits_ & 0xff));
            bits_ >>= 8;
            bitCount_ -= 8;
        }
    }

    QByteArray take()
    {
        if (bitCount_ > 0) {
            bytes_.append(static_cast<char>(bits_ & 0xff));
        }
        return std::move(bytes_);
    }

private:
    QByteArray bytes_;
    quint32 bits_ = 0;
    int bitCount_ = 0;
};

QByteArray encodeLzw(const QByteArray &pixels)
{
    constexpr int clearCode = 256;
    constexpr int endCode = 257;
    constexpr int firstDictionaryCode = 258;
    // Keep the code width at 9 bits and clear before the 10-bit boundary.
    // This trades a little compression for a small, deterministic encoder.
    constexpr int dictionaryLimit = 511;

    LzwBitWriter writer;
    writer.write9(clearCode);
    if (pixels.isEmpty()) {
        writer.write9(endCode);
        return writer.take();
    }

    QHash<quint32, int> dictionary;
    dictionary.reserve(dictionaryLimit - firstDictionaryCode);
    int nextCode = firstDictionaryCode;
    int prefix = static_cast<quint8>(pixels.at(0));

    for (qsizetype i = 1; i < pixels.size(); ++i) {
        const int suffix = static_cast<quint8>(pixels.at(i));
        const quint32 key = (static_cast<quint32>(prefix) << 8) | static_cast<quint32>(suffix);
        const auto found = dictionary.constFind(key);
        if (found != dictionary.constEnd()) {
            prefix = found.value();
            continue;
        }

        writer.write9(prefix);
        if (nextCode < dictionaryLimit) {
            dictionary.insert(key, nextCode++);
        } else {
            writer.write9(clearCode);
            dictionary.clear();
            nextCode = firstDictionaryCode;
        }
        prefix = suffix;
    }

    writer.write9(prefix);
    writer.write9(endCode);
    return writer.take();
}
}

class GifStreamEncoder
{
public:
    GifStreamEncoder(QFile *file, const QSize &size)
        : file_(file), size_(size)
    {
    }

    bool begin()
    {
        if (!size_.isValid() || size_.width() > 65535 || size_.height() > 65535) {
            return false;
        }

        static const char header[] = "GIF89a";
        if (!writeAll(file_, header, 6)
            || !writeLe16(file_, static_cast<quint16>(size_.width()))
            || !writeLe16(file_, static_cast<quint16>(size_.height()))
            || !writeByte(file_, 0x70) // no global color table, 8-bit color resolution
            || !writeByte(file_, 0)
            || !writeByte(file_, 0)) {
            return false;
        }

        static const char loopExtension[] = {
            0x21, static_cast<char>(0xff), 0x0b,
            'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E', '2', '.', '0',
            0x03, 0x01, 0x00, 0x00, 0x00
        };
        return writeAll(file_, loopExtension, sizeof(loopExtension));
    }

    bool addFrame(const QImage &source, int delayCentiseconds)
    {
        if (source.size() != size_) {
            return false;
        }

        const QImage currentFrame = source.convertToFormat(QImage::Format_RGB32);
        QRect changedRect(0, 0, size_.width(), size_.height());
        bool unchanged = false;
        if (!previousFrame_.isNull() && previousFrame_.size() == currentFrame.size()) {
            int left = size_.width();
            int top = size_.height();
            int right = -1;
            int bottom = -1;
            for (int y = 0; y < size_.height(); ++y) {
                const QRgb *currentLine = reinterpret_cast<const QRgb *>(currentFrame.constScanLine(y));
                const QRgb *previousLine = reinterpret_cast<const QRgb *>(previousFrame_.constScanLine(y));
                for (int x = 0; x < size_.width(); ++x) {
                    if (currentLine[x] != previousLine[x]) {
                        left = qMin(left, x);
                        top = qMin(top, y);
                        right = qMax(right, x);
                        bottom = qMax(bottom, y);
                    }
                }
            }
            // An unchanged 1x1 frame preserves timing without duplicating the whole screen.
            unchanged = right < left;
            changedRect = unchanged ? QRect(0, 0, 1, 1)
                                    : QRect(QPoint(left, top), QPoint(right, bottom));
        }
        QImage changedImage;
        if (unchanged && !renderedFrame_.isNull()) {
            changedImage = QImage(1, 1, QImage::Format_RGB32);
            changedImage.setPixel(0, 0, renderedFrame_.pixel(0, 0));
        } else {
            changedImage = currentFrame.copy(changedRect);
        }
        const QuantizedFrame quantized = quantizeAdaptive(changedImage);
        previousFrame_ = currentFrame;

        if (renderedFrame_.isNull()) {
            renderedFrame_ = QImage(size_, QImage::Format_RGB32);
            renderedFrame_.fill(Qt::black);
        }
        for (int y = 0; y < changedRect.height(); ++y) {
            QRgb *renderedLine = reinterpret_cast<QRgb *>(renderedFrame_.scanLine(changedRect.y() + y));
            const uchar *indexedLine = reinterpret_cast<const uchar *>(quantized.pixels.constData())
                                     + y * changedRect.width();
            for (int x = 0; x < changedRect.width(); ++x) {
                renderedLine[changedRect.x() + x] = quantized.colors[indexedLine[x]];
            }
        }

        const quint16 delay = static_cast<quint16>(qBound(1, delayCentiseconds, 65535));
        static const char graphicControlPrefix[] = {0x21, static_cast<char>(0xf9), 0x04, 0x04};
        if (!writeAll(file_, graphicControlPrefix, sizeof(graphicControlPrefix))
            || !writeLe16(file_, delay)
            || !writeByte(file_, 0)
            || !writeByte(file_, 0)
            || !writeByte(file_, 0x2c)
            || !writeLe16(file_, static_cast<quint16>(changedRect.x()))
            || !writeLe16(file_, static_cast<quint16>(changedRect.y()))
            || !writeLe16(file_, static_cast<quint16>(changedRect.width()))
            || !writeLe16(file_, static_cast<quint16>(changedRect.height()))
            || !writeByte(file_, 0x87)) { // local 256-color table
            return false;
        }

        if (!writeAll(file_, quantized.palette.constData(), quantized.palette.size())
            || !writeByte(file_, 8)) {
            return false;
        }

        const QByteArray compressed = encodeLzw(quantized.pixels);
        qsizetype offset = 0;
        while (offset < compressed.size()) {
            const int blockSize = qMin<qsizetype>(255, compressed.size() - offset);
            if (!writeByte(file_, static_cast<quint8>(blockSize))
                || !writeAll(file_, compressed.constData() + offset, blockSize)) {
                return false;
            }
            offset += blockSize;
        }
        return writeByte(file_, 0);
    }

    bool finish()
    {
        return writeByte(file_, 0x3b);
    }

private:
    QFile *file_;
    QSize size_;
    QImage previousFrame_;
    QImage renderedFrame_;
};

GifEncoderWorker::GifEncoderWorker(QObject *parent)
    : QObject(parent)
{
}

GifEncoderWorker::~GifEncoderWorker()
{
    reset(true);
}

void GifEncoderWorker::begin(const QString &outputPath, const QSize &frameSize)
{
    reset(true);
    outputPath_ = outputPath;
    temporaryPath_ = outputPath + QStringLiteral(".part");
    QFile::remove(temporaryPath_);

    file_ = std::make_unique<QFile>(temporaryPath_);
    if (!file_->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QString error = file_->errorString();
        reset(true);
        emit sigReady(false, error);
        return;
    }

    encoder_ = std::make_unique<GifStreamEncoder>(file_.get(), frameSize);
    if (!encoder_->begin()) {
        reset(true);
        emit sigReady(false, QStringLiteral("无法创建 GIF 文件"));
        return;
    }

    active_ = true;
    emit sigReady(true, QString());
}

void GifEncoderWorker::addFrame(const QImage &image, int delayCentiseconds)
{
    if (!active_ || !encoder_ || !encoder_->addFrame(image, delayCentiseconds)) {
        const QString failedPath = outputPath_;
        const QString error = file_ ? file_->errorString() : QStringLiteral("GIF 编码器未启动");
        reset(true);
        emit sigFinished(false, failedPath, error.isEmpty() ? QStringLiteral("GIF 帧编码失败") : error);
        return;
    }
    emit sigFrameWritten();
}

void GifEncoderWorker::finish()
{
    if (!active_ || !encoder_) {
        return;
    }

    const QString finalPath = outputPath_;
    const bool trailerWritten = encoder_->finish();
    encoder_.reset();
    const bool flushed = file_->flush();
    file_->close();
    file_.reset();
    active_ = false;

    if (!trailerWritten || !flushed) {
        QFile::remove(temporaryPath_);
        emit sigFinished(false, finalPath, QStringLiteral("写入 GIF 文件失败"));
        return;
    }

    if (QFile::exists(finalPath) && !QFile::remove(finalPath)) {
        QFile::remove(temporaryPath_);
        emit sigFinished(false, finalPath, QStringLiteral("无法覆盖已有文件"));
        return;
    }
    if (!QFile::rename(temporaryPath_, finalPath)) {
        QFile::remove(temporaryPath_);
        emit sigFinished(false, finalPath, QStringLiteral("无法完成 GIF 文件保存"));
        return;
    }

    temporaryPath_.clear();
    emit sigFinished(true, finalPath, QString());
}

void GifEncoderWorker::cancel()
{
    reset(true);
}

void GifEncoderWorker::reset(bool removeTemporaryFile)
{
    active_ = false;
    encoder_.reset();
    if (file_) {
        file_->close();
        file_.reset();
    }
    if (removeTemporaryFile && !temporaryPath_.isEmpty()) {
        QFile::remove(temporaryPath_);
    }
    outputPath_.clear();
    temporaryPath_.clear();
}
