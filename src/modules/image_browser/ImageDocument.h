#pragma once

#include "ImageConversionOptions.h"
#include "core/imaging/ImageLimits.h"

#include <QDateTime>
#include <QImage>
#include <QRect>
#include <QString>
#include <QVector>
#include <QColor>
#include <QPolygonF>

namespace ImageBrowser {

constexpr qint64 MaxEditablePixels = ImageLimits::MaxEditablePixels;

struct EditOperation {
    enum Kind { Crop, RotateLeft, RotateRight, FlipHorizontal, FlipVertical, Resize,
                Annotate, Perspective, Mosaic, Watermark, Sticker, Filter, AdjustColor, Cutout };
    Kind kind = Crop;
    QRect rect;
    QSize size;
    QColor color = Qt::red;
    QString text;
    QImage asset;
    QPolygonF corners;
    int variant = 0;
    int strength = 12;
    int brightness = 0;
    int contrast = 0;
    int saturation = 0;
    int temperature = 0;
    int tint = 0;
    int opacity = 100;
    int feather = 1;
    // Watermark: 0 custom region, 1 bottom right, 2 bottom left,
    // 3 top right, 4 top left, 5 center, 6 tiled.
    int margin = 24;
    QPointF start, end;
    bool directDrawing = false;
    // Stored in image coordinates for replay. Canvas converts from screen pixels.
    qreal brushDiameter = 24;
    QPolygonF brushPoints;
};

struct ImageResult {
    QImage image;
    QSize originalSize;
    QString error;
    bool limited = false;
    bool animated = false;
    bool animationSupported = false;
    QDateTime modified;
    qint64 fileSize = 0;
};

struct SaveResult {
    bool success = false;
    QString error;
    QImage image;
};

class ImageDocument {
public:
    QString path;
    QImage original;
    QImage current;
    QVector<EditOperation> operations;
    int position = 0;
    bool frozenFrame = false;
    bool limited = false;
    QDateTime modified;
    qint64 fileSize = 0;

    bool dirty() const { return frozenFrame || position != 0; }
    bool canUndo() const { return position > 0; }
    bool canRedo() const { return position < operations.size(); }
    void load(const QString& file, const ImageResult& result);
    void append(const EditOperation& operation);
    static ImageResult read(const QString& path, const QSize& thumbnail = {});
    static ImageResult readFrame(const QString& path, int frame);
    static ImageResult render(const QImage& original,
                              const QVector<EditOperation>& operations, int count);
    static SaveResult save(const QString& path, const QImage& image, const QByteArray& format,
                           const QDateTime& expectedModified = {}, qint64 expectedSize = -1,
                           const ImageConversionOptions& options = {});
    static QString suggestedPath(const QString& source, const QByteArray& format = "png");
    bool sourceUnchanged() const;
};

} // namespace ImageBrowser
