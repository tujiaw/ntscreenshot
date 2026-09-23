#pragma once

#include <QElapsedTimer>
#include <QWidget>

class GifEncoderWorker;
class GifRecorderControlPanel;
class QImage;
class QKeyEvent;
class QThread;
class QTimer;

class GifRecorderWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GifRecorderWidget(const QRect &captureRect, const QString &outputDirectory,
                               QWidget *parent = nullptr);
    ~GifRecorderWidget() override;

    void stopRecording();

signals:
    void sigWindowClosed();
    void sigBeginEncoding(const QString &path, const QSize &frameSize);
    void sigEncodeFrame(const QImage &image, int delayCentiseconds);
    void sigFinishEncoding();
    void sigCancelEncoding();

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onRecord();
    void onStop();
    void onCopy();
    void onOpen();
    void onCancel();
    void onEncoderReady(bool success, const QString &error);
    void onCaptureTimer();
    void onFrameWritten();
    void onEncodingFinished(bool success, const QString &path, const QString &error);

private:
    enum class State { Preparing, Starting, Recording, Encoding, Closed };
    enum class CompletionAction { None, Copy, Open };

    QRect contentRect() const;
    void positionControlPanel();
    void closeRecorder();
    QString defaultOutputPath() const;

    QRect captureRect_;
    QString outputDirectory_;
    QString outputPath_;
    GifRecorderControlPanel *controlPanel_ = nullptr;
    QTimer *captureTimer_ = nullptr;
    QThread *encoderThread_ = nullptr;
    GifEncoderWorker *encoderWorker_ = nullptr;
    QElapsedTimer elapsed_;
    State state_ = State::Preparing;
    CompletionAction completionAction_ = CompletionAction::None;
    int borderWidth_ = 3;
    int pendingFrames_ = 0;
    int capturedFrames_ = 0;
    qint64 lastQueuedAtMs_ = 0;
};
