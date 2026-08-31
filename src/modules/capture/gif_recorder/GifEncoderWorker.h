#pragma once

#include <QObject>
#include <QSize>
#include <QString>
#include <memory>

class QFile;
class QImage;
class GifStreamEncoder;

class GifEncoderWorker : public QObject
{
    Q_OBJECT

public:
    explicit GifEncoderWorker(QObject *parent = nullptr);
    ~GifEncoderWorker() override;

public slots:
    void begin(const QString &outputPath, const QSize &frameSize);
    void addFrame(const QImage &image, int delayCentiseconds);
    void finish();
    void cancel();

signals:
    void sigReady(bool success, const QString &error);
    void sigFrameWritten();
    void sigFinished(bool success, const QString &outputPath, const QString &error);

private:
    void reset(bool removeTemporaryFile);

    QString outputPath_;
    QString temporaryPath_;
    std::unique_ptr<QFile> file_;
    std::unique_ptr<GifStreamEncoder> encoder_;
    bool active_ = false;
};
