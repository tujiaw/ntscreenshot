#pragma once

#include <QWidget>
#include <QPixmap>
#include <QTimer>
#include <QPoint>
#include <QRegion>
#include <QImage>
#include <QVector>
#include <QQueue>
#include <QThreadPool>
#include <atomic>
#include <memory>
#include <utility>

class LongScreenshotControlPanel;
class WindowManager;

class LongScreenshotWidget : public QWidget
{
    Q_OBJECT

public:
    LongScreenshotWidget(WindowManager* windowManager, const QRect& captureRect,
                         std::shared_ptr<QPixmap> originScreen, QWidget* parent = nullptr);
    ~LongScreenshotWidget();

    void stopCapture(); // Public method to stop and finish screenshot

signals:
    void sigScreenshotFinished(const QPixmap& pixmap);
    void sigWindowClosed();

protected:
    void paintEvent(QPaintEvent*) override;

private slots:
    void onCaptureTimer();
    void onFinish();
    void onCancel();

private:
    WindowManager* windowManager_ = nullptr;
    struct CaptureFrame {
        QImage image;
        int shiftFromPrev;
        double matchScore;
    };

    void initUI();
    void startCapture();
    void captureCurrentFrame();
    void processNextFrame();
    void finishCapture();
    static bool hasGlobalMotion(const QImage& current, const QImage& last);
    static bool shouldAppendFrame(const QImage& currentImage, const QImage& prevImage, int* shiftFromPrev, double* matchScore);
    QPixmap stitchImages();
    static std::pair<int, double> estimateScrollShift(const QImage& previous, const QImage& current, int minShift, int maxShift);
    static int chooseSeamRows(const QImage& result, const QImage& source, int targetY, int sourceY);
    static bool isReliableShift(int shift, double score, int imageHeight);
    void updateInfoLabel();
    void positionControlPanel();

private:
    QRect captureRect_;
    std::shared_ptr<QPixmap> originScreen_;
    QVector<CaptureFrame> capturedFrames_;
    QTimer* captureTimer_;
    QImage lastCapturedImage_;
    QQueue<QImage> pendingFrames_;

    LongScreenshotControlPanel* controlPanel_ = nullptr;
    int borderWidth_;
    int frameSkipCounter_;
    bool analysisInProgress_ = false;  // 防止 ORB 分析重入，避免主线程积压
    bool finishRequested_ = false;
    bool panelIntersectsCapture_ = false;
    bool panelPositioned_ = false;
    bool captureOverloaded_ = false;
    int unmatchedMotionCount_ = 0;
    bool completionRequested_ = false;
    std::shared_ptr<std::atomic_bool> lifetimeToken_ = std::make_shared<std::atomic_bool>(true);
    static constexpr int CAPTURE_INTERVAL_MS = 150;
    static constexpr int MIN_APPEND_HEIGHT_PX = 10;
    static constexpr int MAX_CAPTURED_FRAMES = 220;
    static constexpr int MAX_PENDING_FRAMES = 12;
    static constexpr int FRAME_SKIP_RATE = 1;
};
