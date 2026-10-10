#pragma once

#include <QDateTime>
#include <QImage>
#include <QRect>
#include <QString>
#include <QVector>

namespace ImageBrowser {

constexpr qint64 MaxEditablePixels = 40000000;

struct EditOperation {
    enum Kind { Crop, RotateLeft, RotateRight, FlipHorizontal, FlipVertical, Resize };
    Kind kind = Crop;
    QRect rect;
    QSize size;
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
                           const QDateTime& expectedModified = {}, qint64 expectedSize = -1);
    static QString suggestedPath(const QString& source, const QByteArray& format = "png");
    bool sourceUnchanged() const;
};

} // namespace ImageBrowser
