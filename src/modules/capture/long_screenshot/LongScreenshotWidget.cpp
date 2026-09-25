#include "LongScreenshotWidget.h"
#include "LongScreenshotControlPanel.h"
#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QClipboard>
#include <QApplication>
#include <QScreen>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <limits>
#include "core/platform/Util.h"
#include "core/theme/ThemeManager.h"
#include "core/imaging/ImageMatcher.h"
#include "shared/ui/TipsWidget.h"
#include "modules/capture/pin/PinWidget.h"
#include "app/WindowManager.h"

// 优化相关常量
static constexpr int MAX_STITCH_HEIGHT = 60000;   // 拼接图片最大高度安全限制

LongScreenshotWidget::LongScreenshotWidget(WindowManager* windowManager, const QRect& captureRect, std::shared_ptr<QPixmap> originScreen, QWidget* parent)
    : QWidget(parent)
    , windowManager_(windowManager)
    , captureRect_(captureRect)
    , originScreen_(originScreen)
    , captureTimer_(new QTimer(this))
    , controlPanel_(nullptr)
    , borderWidth_(4)
    , frameSkipCounter_(0)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setGeometry(QRect(captureRect_.topLeft() + Util::desktopRect().topLeft(), captureRect_.size()));

    connect(captureTimer_, &QTimer::timeout, this, &LongScreenshotWidget::onCaptureTimer);

    // 创建悬浮控制面板
    controlPanel_ = new LongScreenshotControlPanel();
    connect(controlPanel_, &LongScreenshotControlPanel::sigCancelClicked, this, &LongScreenshotWidget::onCancel, Qt::QueuedConnection);
    connect(controlPanel_, &LongScreenshotControlPanel::sigFinishClicked, this, &LongScreenshotWidget::onFinish, Qt::QueuedConnection);

    initUI();
    // 延迟一小段时间，确保窗口控件初始化完成再开始截图
    QTimer::singleShot(100, this, [this]() {
        startCapture();
    });
}

LongScreenshotWidget::~LongScreenshotWidget()
{
    lifetimeToken_->store(false);
    if (captureTimer_ && captureTimer_->isActive()) {
        captureTimer_->stop();
    }
    if (controlPanel_) {
        controlPanel_->deleteLater();
    }
}

void LongScreenshotWidget::initUI()
{
    QRect frameRect = rect();
    QRegion outerRegion(frameRect);
    QRegion innerRegion(frameRect.adjusted(borderWidth_, borderWidth_, -borderWidth_, -borderWidth_));
    setMask(outerRegion.subtracted(innerRegion));
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setWindowOpacity(1.0);
}

void LongScreenshotWidget::updateInfoLabel()
{
    if (!controlPanel_) {
        return;
    }
    // Keep the panel's content width stable while capturing. Appending and
    // removing a warning here changes its size/position and makes the hint
    // visibly jump between frames.
    QString text = QString(QStringLiteral("已截取: %1 帧")).arg(capturedFrames_.size());
    controlPanel_->setInfoText(text);

    if (!finishRequested_ && !controlPanel_->isVisible()) {
        controlPanel_->show();
        controlPanel_->raise();
    }
}

void LongScreenshotWidget::positionControlPanel()
{
    if (!controlPanel_) {
        return;
    }
    controlPanel_->adjustSize();
    const QRect screenRect = Util::desktopRect();
    const QRect globalCaptureRect(captureRect_.topLeft() + screenRect.topLeft(), captureRect_.size());

    const QSize size = controlPanel_->size();
    const QRect candidates[] = {
        QRect(QPoint(globalCaptureRect.right() - size.width() + 1,
                     globalCaptureRect.top() - size.height() - 6), size),
        QRect(QPoint(globalCaptureRect.right() - size.width() + 1,
                     globalCaptureRect.bottom() + 7), size),
        QRect(QPoint(globalCaptureRect.right() + 7, globalCaptureRect.top()), size),
        QRect(QPoint(globalCaptureRect.left() - size.width() - 6, globalCaptureRect.top()), size)
    };
    for (const QRect& candidate : candidates) {
        if (screenRect.contains(candidate) && !candidate.intersects(globalCaptureRect)) {
            panelIntersectsCapture_ = false;
            controlPanel_->move(candidate.topLeft());
            panelPositioned_ = true;
            return;
        }
    }
    // No outside position fits. Keep controls usable but hide them for every grab.
    panelIntersectsCapture_ = true;
    const int x = qBound(screenRect.left(), globalCaptureRect.left() + 6,
                         screenRect.right() - size.width() + 1);
    const int y = qBound(screenRect.top(), globalCaptureRect.top() + 6,
                         screenRect.bottom() - size.height() + 1);
    controlPanel_->move(x, y);
    panelPositioned_ = true;
}

void LongScreenshotWidget::startCapture()
{
    if (completionRequested_ || finishRequested_) {
        return;
    }

    capturedFrames_.clear();
    captureCurrentFrame();
    captureTimer_->start(CAPTURE_INTERVAL_MS);

    // 初始化并显示控制面板信息
    if (controlPanel_) {
        // Use the final text shape before positioning, then freeze the
        // top-level panel size. Resizing a floating native window while the
        // frame count changes can make Windows re-center it visually.
        controlPanel_->setInfoText(QStringLiteral("已截取: 0 帧"));
        controlPanel_->setFixedSize(controlPanel_->size());
        positionControlPanel();
        controlPanel_->show();
        controlPanel_->raise();
    }

    show();
    raise();
}

void LongScreenshotWidget::captureCurrentFrame()
{
    if (completionRequested_ || finishRequested_) {
        return;
    }

    if (capturedFrames_.size() >= MAX_CAPTURED_FRAMES) {
        onFinish();
        return;
    }

    // 截图必须在主线程执行
    QRect innerRect = captureRect_.adjusted(borderWidth_, borderWidth_, -borderWidth_, -borderWidth_);
    const bool restorePanel = panelIntersectsCapture_ && controlPanel_ && controlPanel_->isVisible();
    if (restorePanel) controlPanel_->hide();
    QImage currentImage = Util::grabDesktopImage(innerRect);
    if (restorePanel) {
        controlPanel_->show();
        controlPanel_->raise();
    }
    if (currentImage.isNull()) {
        return;
    }

    if (currentImage.format() != QImage::Format_RGB32 && currentImage.format() != QImage::Format_ARGB32) {
        currentImage = currentImage.convertToFormat(QImage::Format_RGB32);
    }

    if (capturedFrames_.isEmpty() && !analysisInProgress_ && pendingFrames_.isEmpty()) {
        capturedFrames_.push_back({currentImage, 0, 0.0});
        lastCapturedImage_ = currentImage;
        return;
    }

    if (pendingFrames_.size() >= MAX_PENDING_FRAMES) {
        captureOverloaded_ = true;
        captureTimer_->stop();
        updateInfoLabel();
        return;
    }
    pendingFrames_.enqueue(currentImage);
    processNextFrame();
}

void LongScreenshotWidget::processNextFrame()
{
    if (completionRequested_ || analysisInProgress_) return;
    if (pendingFrames_.isEmpty()) {
        if (finishRequested_) finishCapture();
        return;
    }

    // Process captured frames in order, retaining the overlap between each pair.
    analysisInProgress_ = true;
    QImage currentImage = pendingFrames_.dequeue();
    QImage prevImage = lastCapturedImage_;
    LongScreenshotWidget* self = this;
    auto lifetimeToken = lifetimeToken_;

    QThreadPool::globalInstance()->start([currentImage, prevImage, self, lifetimeToken]() {
        int shift = 0;
        double score = 0.0;
        bool motion = LongScreenshotWidget::hasGlobalMotion(currentImage, prevImage);
        bool accepted = motion && LongScreenshotWidget::shouldAppendFrame(currentImage, prevImage, &shift, &score);

        if (!lifetimeToken->load() || !qApp) {
            return;
        }

        QMetaObject::invokeMethod(qApp, [currentImage, shift, score, accepted, motion, self, lifetimeToken]() {
            if (!lifetimeToken->load()) {
                return;
            }

            self->analysisInProgress_ = false;
            if (self->completionRequested_) {
                return;
            }
            if (!accepted) {
                if (motion) ++self->unmatchedMotionCount_;
                qDebug() << "[LongScr] frame rejected, frames so far:" << self->capturedFrames_.size();
            } else {
                self->unmatchedMotionCount_ = 0;
                self->capturedFrames_.push_back({currentImage, shift, score});
                self->lastCapturedImage_ = currentImage;
                qDebug() << "frame count:" << self->capturedFrames_.size();
            }
            self->updateInfoLabel();
            if (self->captureOverloaded_ && !self->finishRequested_
                && self->pendingFrames_.size() <= MAX_PENDING_FRAMES / 3) {
                self->captureOverloaded_ = false;
                self->captureTimer_->start(CAPTURE_INTERVAL_MS);
            }
            if (self->capturedFrames_.size() >= MAX_CAPTURED_FRAMES) {
                self->pendingFrames_.clear();
                self->finishRequested_ = true;
                self->captureTimer_->stop();
            }
            self->processNextFrame();
        }, Qt::QueuedConnection);
    });
}

bool LongScreenshotWidget::hasGlobalMotion(const QImage& current, const QImage& last)
{
    if (current.size() != last.size()) {
        return true;
    }

    int width = current.width();
    int height = current.height();
    if (width <= 0 || height <= 0) return false;

    // 检查中心区域的像素变化，以此判断画面是否在滚动
    // 优化后的容错策略：检查更大的区域（70%）以避免因为中间出现大块空白而导致检测失败
    int xMargin = width * 0.15; // 检查中心70%区域
    int yStart = height * 0.15;
    int yEnd = height * 0.85;
    int xStep = qMax(1, (width - 2 * xMargin) / 20); // 保持足够的采样点
    int yStep = qMax(1, (yEnd - yStart) / 20);

    int changed = 0;
    int sampled = 0;

    // 使用原始数据指针提升像素访问速度
    for (int y = yStart; y < yEnd; y += yStep) {
        const QRgb* curLine = reinterpret_cast<const QRgb*>(current.constScanLine(y));
        const QRgb* lastLine = reinterpret_cast<const QRgb*>(last.constScanLine(y));

        for (int x = xMargin; x < width - xMargin; x += xStep) {
            QRgb a = curLine[x];
            QRgb b = lastLine[x];

            // 计算RGB绝对差值的总和
            int diff = std::abs(qRed(a) - qRed(b)) + std::abs(qGreen(a) - qGreen(b)) + std::abs(qBlue(a) - qBlue(b));
            if (diff > 30) { // 较低的阈值，使滚动检测对像素变化更敏感
                ++changed;
            }
            ++sampled;
        }
    }

    if (sampled == 0) return false;
    double ratio = static_cast<double>(changed) / sampled;
    // 表格场景下大量固定列（如"中国/墨西哥/徐州/照章"）导致像素变化率偏低（约1.2~1.9%），
    // 原 2% 阈值会将真实滚动误判为静止。降至 0.8% 以覆盖此场景。
    // 即便误触发，后续 isReliableShift 也会通过 shift/score 验证过滤掉假运动。
    static constexpr double MOTION_RATIO_THRESHOLD = 0.008;
    bool hasMotion = ratio > MOTION_RATIO_THRESHOLD;
    if (!hasMotion) {
        qDebug() << "[LongScr] hasGlobalMotion=false, ratio=" << ratio
                 << "(need >" << MOTION_RATIO_THRESHOLD << ")";
    }
    return hasMotion;
}

bool LongScreenshotWidget::shouldAppendFrame(const QImage& currentImage, const QImage& prevImage, int* shiftFromPrev, double* matchScore)
{
    if (!shiftFromPrev || !matchScore) {
        return false;
    }

    *shiftFromPrev = 0;
    *matchScore = 0.0;

    if (!hasGlobalMotion(currentImage, prevImage)) {
        qDebug() << "[LongScr] skip: no global motion";
        return false;
    }

    int imageHeight = qMin(currentImage.height(), prevImage.height());
    if (imageHeight <= MIN_APPEND_HEIGHT_PX) {
        return false;
    }

    int minShift = -10;
    int maxShift = imageHeight * 3 / 4;

    auto [shift, score] = estimateScrollShift(prevImage, currentImage, minShift, maxShift);

    if (!isReliableShift(shift, score, imageHeight)) {
        qDebug() << "[LongScr] skip: unreliable shift=" << shift << "score=" << score << "imageHeight=" << imageHeight;
        return false;
    }

    *shiftFromPrev = shift;
    *matchScore = score;
    return true;
}

QPixmap LongScreenshotWidget::stitchImages()
{
    if (capturedFrames_.isEmpty()) {
        return QPixmap();
    }
    if (capturedFrames_.size() == 1) {
        return QPixmap::fromImage(capturedFrames_.first().image);
    }

    // 1. 计算拼接后图片的总高度
    int width = capturedFrames_.first().image.width();
    int totalHeight = capturedFrames_.first().image.height();
    for (int i = 1; i < capturedFrames_.size(); ++i) {
        totalHeight += capturedFrames_[i].shiftFromPrev;
    }

    if (totalHeight > MAX_STITCH_HEIGHT) {
        totalHeight = MAX_STITCH_HEIGHT; // 限制最大高度
    }

    QImage result(width, totalHeight, QImage::Format_RGB32);
    result.fill(Qt::white);
    const QImage first = capturedFrames_.first().image.convertToFormat(QImage::Format_RGB32);
    int currentY = 0;
    for (int row = 0; row < first.height() && row < totalHeight; ++row) {
        std::memcpy(result.scanLine(row), first.constScanLine(row), width * sizeof(QRgb));
    }
    currentY += first.height();

    for (int i = 1; i < capturedFrames_.size(); ++i) {
        const auto& frame = capturedFrames_[i];
        int shift = frame.shiftFromPrev;

        if (shift <= 0) continue;
        if (currentY + shift > totalHeight) break;

        const QImage img = frame.image.convertToFormat(QImage::Format_RGB32);
        if (img.width() != width || shift >= img.height()) continue;

        // 计算当前帧新增内容的来源位置
        // shift 表示当前帧底部“新增”了多少个像素的高度
        int sourceY = img.height() - shift;

        // Move the cut to the best-matching row in the overlap, avoiding text
        // baselines. Copy pixels rather than alpha-blending two text renders.
        const int seamRows = chooseSeamRows(result, img, currentY, sourceY);
        for (int row = -seamRows; row < shift; ++row) {
            std::memcpy(result.scanLine(currentY + row),
                        img.constScanLine(sourceY + row), width * sizeof(QRgb));
        }

        currentY += shift;
    }

    return QPixmap::fromImage(result);
}

std::pair<int, double> LongScreenshotWidget::estimateScrollShift(const QImage& previous, const QImage& current, int minShift, int maxShift)
{
    return ImageMatcher::estimateScrollShift(previous, current, minShift, maxShift);
}

int LongScreenshotWidget::chooseSeamRows(const QImage& result, const QImage& source, int targetY, int sourceY)
{
    const int limit = std::min({24, sourceY, targetY});
    int bestRows = 0;
    double bestCost = std::numeric_limits<double>::max();
    const int x0 = result.width() / 10;
    const int x1 = result.width() * 9 / 10;
    for (int rows = 1; rows <= limit; ++rows) {
        const QRgb* oldLine = reinterpret_cast<const QRgb*>(result.constScanLine(targetY - rows));
        const QRgb* newLine = reinterpret_cast<const QRgb*>(source.constScanLine(sourceY - rows));
        double difference = 0;
        int samples = 0;
        for (int x = x0; x < x1; x += 3) {
            difference += std::abs(qGray(oldLine[x]) - qGray(newLine[x]));
            ++samples;
        }
        const double cost = samples ? difference / samples : 255.0;
        if (cost < bestCost) {
            bestCost = cost;
            bestRows = rows;
        }
    }
    return bestCost <= 12.0 ? bestRows : 0;
}

bool LongScreenshotWidget::isReliableShift(int shift, double score, int imageHeight)
{
    if (shift < MIN_APPEND_HEIGHT_PX) {
        qDebug() << "[LongScr] isReliableShift=false: shift" << shift << "< MIN" << MIN_APPEND_HEIGHT_PX;
        return false;
    }
    if (shift >= imageHeight - 10) {
        qDebug() << "[LongScr] isReliableShift=false: shift" << shift << ">= imageHeight-10 =" << (imageHeight - 10);
        return false;
    }
    if (score >= 0.15) {
        qDebug() << "[LongScr] isReliableShift=false: score" << score << ">= 0.15";
        return false;
    }
    return true;
}

void LongScreenshotWidget::onCaptureTimer()
{
    captureCurrentFrame();
}

void LongScreenshotWidget::stopCapture()
{
    onFinish();
}

void LongScreenshotWidget::onFinish()
{
    if (completionRequested_ || finishRequested_) {
        return;
    }
    finishRequested_ = true;
    captureTimer_->stop();
    if (controlPanel_) {
        controlPanel_->hide();
    }
    // The final screen state must be queued behind in-flight analysis.
    QRect innerRect = captureRect_.adjusted(borderWidth_, borderWidth_, -borderWidth_, -borderWidth_);
    QImage finalFrame = Util::grabDesktopImage(innerRect);
    if (!finalFrame.isNull() && !capturedFrames_.isEmpty()) {
        pendingFrames_.enqueue(finalFrame.convertToFormat(QImage::Format_RGB32));
    }
    processNextFrame();
}

void LongScreenshotWidget::finishCapture()
{
    if (completionRequested_) return;
    completionRequested_ = true;

    if (capturedFrames_.isEmpty()) {
        TipsWidget::popup(this, QStringLiteral("未截取到图像"), 2, 0, true);
        emit sigWindowClosed();
        close();
        return;
    }

    QPixmap finalImage = stitchImages();
    if (!finalImage.isNull()) {
        QApplication::clipboard()->setPixmap(finalImage);
        emit sigScreenshotFinished(finalImage);

        QPoint pos = captureRect_.topLeft() + Util::desktopRect().topLeft();
        if (PinWidget::hasBorder(windowManager_->setting())) {
            pos -= QPoint(1, 1);
        }

        emit sigWindowClosed();
        close();
        // 显示最终的长截图结果贴图
        WindowManager* windowManager = windowManager_;
        QTimer::singleShot(0, [windowManager, finalImage, pos]() { PinWidget::popup(windowManager, finalImage, pos); });

        // 弹出成功提示
        TipsWidget::popup(nullptr, QString(QStringLiteral("长截图完成\n已复制到剪贴板")), 3, 0, true);
    } else {
        TipsWidget::popup(this, QStringLiteral("拼接失败"), 2, 0, true);
        emit sigWindowClosed();
        close();
    }
}

void LongScreenshotWidget::onCancel()
{
    if (completionRequested_) {
        return;
    }
    completionRequested_ = true;
    pendingFrames_.clear();

    captureTimer_->stop();

    // 隐藏控制面板
    if (controlPanel_) {
        controlPanel_->hide();
    }

    emit sigWindowClosed();
    // 直接关闭窗口，不保存任何内容
    close();
}

void LongScreenshotWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);

    // 现代外观：使用主题强调色的密集虚线边框
    QPen pen(ThemeManager::tokens().accent, borderWidth_);
    pen.setStyle(Qt::DashLine);
    pen.setDashPattern({4, 2}); // 4像素实线，2像素空白（更加密集）
    painter.setPen(pen);
    painter.drawRect(rect());

}
