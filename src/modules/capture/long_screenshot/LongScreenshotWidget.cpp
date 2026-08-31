#include "LongScreenshotWidget.h"
#include "LongScreenshotControlPanel.h"
#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QClipboard>
#include <QApplication>
#include <QScreen>
#include <cmath>
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
    QString text = QString(QStringLiteral("已截取: %1 帧")).arg(capturedFrames_.size());
    controlPanel_->setInfoText(text);

    positionControlPanel();

    if (!controlPanel_->isVisible()) {
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

    // 默认悬浮在截图区域右上角外侧
    int x = globalCaptureRect.right() - controlPanel_->width();
    int y = globalCaptureRect.top() - controlPanel_->height() - 6;
    // 截图区域贴近屏幕顶部时，面板改到区域内侧，避免跑出屏幕
    if (y < screenRect.top()) {
        y = globalCaptureRect.top() + 6;
    }
    if (x + controlPanel_->width() > screenRect.right()) {
        x = screenRect.right() - controlPanel_->width();
    }
    if (x < screenRect.left()) {
        x = screenRect.left();
    }
    controlPanel_->move(x, y);
}

void LongScreenshotWidget::startCapture()
{
    if (completionRequested_) {
        return;
    }

    capturedFrames_.clear();
    captureCurrentFrame();
    captureTimer_->start(CAPTURE_INTERVAL_MS);

    // 初始化并显示控制面板信息
    if (controlPanel_) {
        controlPanel_->setInfoText(QStringLiteral("截长图"));
        positionControlPanel();
        controlPanel_->show();
        controlPanel_->raise();
    }

    show();
    raise();
}

void LongScreenshotWidget::captureCurrentFrame()
{
    if (completionRequested_) {
        return;
    }

    if (capturedFrames_.size() >= MAX_CAPTURED_FRAMES) {
        onFinish();
        return;
    }

    // 上一帧的 ORB/相位相关分析还未完成时，跳过本帧。
    // 避免在主线程积压耗时 250ms 的分析任务。
    if (analysisInProgress_) {
        return;
    }

    // 截图必须在主线程执行
    QRect innerRect = captureRect_.adjusted(borderWidth_, borderWidth_, -borderWidth_, -borderWidth_);
    QImage currentImage = Util::grabDesktopImage(innerRect);
    if (currentImage.isNull()) {
        return;
    }

    if (currentImage.format() != QImage::Format_RGB32 && currentImage.format() != QImage::Format_ARGB32) {
        currentImage = currentImage.convertToFormat(QImage::Format_RGB32);
    }

    if (capturedFrames_.isEmpty()) {
        capturedFrames_.push_back({currentImage, 0, 0.0});
        lastCapturedImage_ = currentImage;
        return;
    }

    // ORB + 相位相关耗时 200~300ms，放到全局线程池避免阻塞主线程。
    // QThreadPool 属于 Qt Core，无需额外模块。
    analysisInProgress_ = true;
    QImage prevImage = lastCapturedImage_;
    LongScreenshotWidget* self = this;
    auto lifetimeToken = lifetimeToken_;

    QThreadPool::globalInstance()->start([currentImage, prevImage, self, lifetimeToken]() {
        int shift = 0;
        double score = 0.0;
        bool accepted = LongScreenshotWidget::shouldAppendFrame(currentImage, prevImage, &shift, &score);

        if (!lifetimeToken->load() || !qApp) {
            return;
        }

        QMetaObject::invokeMethod(qApp, [currentImage, shift, score, accepted, self, lifetimeToken]() {
            if (!lifetimeToken->load()) {
                return;
            }

            self->analysisInProgress_ = false;
            if (self->completionRequested_) {
                return;
            }
            if (!accepted) {
                qDebug() << "[LongScr] frame rejected, frames so far:" << self->capturedFrames_.size();
                return;
            }
            self->capturedFrames_.push_back({currentImage, shift, score});
            self->lastCapturedImage_ = currentImage;
            self->updateInfoLabel();
            qDebug() << "frame count:" << self->capturedFrames_.size();
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

    QImage result(width, totalHeight, QImage::Format_ARGB32);
    result.fill(Qt::white);

    QPainter painter(&result);
    // 使用直接覆盖模式(Source)，接缝处的羽化由我们手动处理
    painter.setCompositionMode(QPainter::CompositionMode_Source);

    // 绘制第一帧作为基底
    int currentY = 0;
    painter.drawImage(0, 0, capturedFrames_.first().image);
    currentY += capturedFrames_.first().image.height();

    for (int i = 1; i < capturedFrames_.size(); ++i) {
        const auto& frame = capturedFrames_[i];
        int shift = frame.shiftFromPrev;

        if (shift <= 0) continue;
        if (currentY + shift > totalHeight) break;

        const QImage& img = frame.image;

        // 计算当前帧新增内容的来源位置
        // shift 表示当前帧底部“新增”了多少个像素的高度
        int sourceY = img.height() - shift;

        // 接缝羽化：限制在 4px 以内。
        // 过大的羽化区域（旧值 24px）会跨越表格行边界，导致相邻行内容混叠出现"双影"。
        int blendHeight = qBound(0, shift / 8, 4);

        if (blendHeight > 0) {
            blendSeamRows(result, img, currentY - blendHeight, sourceY - blendHeight, blendHeight);
        }

        QRect srcRect(0, sourceY, img.width(), shift);
        QRect dstRect(0, currentY, img.width(), shift);
        painter.drawImage(dstRect, img, srcRect);

        currentY += shift;
    }

    painter.end();
    return QPixmap::fromImage(result);
}

std::pair<int, double> LongScreenshotWidget::estimateScrollShift(const QImage& previous, const QImage& current, int minShift, int maxShift)
{
    return ImageMatcher::estimateScrollShift(previous, current, minShift, maxShift);
}

void LongScreenshotWidget::blendSeamRows(QImage& result, const QImage& source, int targetY, int sourceY, int rowCount) const
{
    // 安全检查，防止越界
    if (rowCount <= 0 || targetY < 0 || sourceY < 0) return;
    int width = qMin(result.width(), source.width());
    int maxRows = qMin(rowCount, qMin(result.height() - targetY, source.height() - sourceY));

    if (maxRows <= 0) return;

    for (int row = 0; row < maxRows; ++row) {
        // 透明度 Alpha 从 0.0 (保留原有图像) 渐变到 1.0 (使用新图像内容)
        // 使得接缝处过渡更加平滑
        float alpha = (float)(row + 1) / (float)(maxRows + 1);

        int dstY = targetY + row;
        int srcY = sourceY + row;

        QRgb* dstLine = reinterpret_cast<QRgb*>(result.scanLine(dstY));
        const QRgb* srcLine = reinterpret_cast<const QRgb*>(source.constScanLine(srcY));

        for (int x = 0; x < width; ++x) {
            QRgb up = dstLine[x];
            QRgb down = srcLine[x];

            int r = (int)(qRed(up) * (1.0f - alpha) + qRed(down) * alpha);
            int g = (int)(qGreen(up) * (1.0f - alpha) + qGreen(down) * alpha);
            int b = (int)(qBlue(up) * (1.0f - alpha) + qBlue(down) * alpha);

            dstLine[x] = qRgb(r, g, b);
        }
    }
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
    if (completionRequested_) {
        return;
    }
    completionRequested_ = true;

    captureTimer_->stop();

    // 隐藏控制面板
    if (controlPanel_) {
        controlPanel_->hide();
    }

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

    painter.setPen(Qt::white);
    painter.setRenderHint(QPainter::TextAntialiasing);

    // 绘制半透明背景，确保信息文字清晰可读
    QString infoText = QString(QStringLiteral("已截取: %1 帧 | 停止滚动以完成")).arg(capturedFrames_.size());
    QFont font = painter.font();
    font.setPixelSize(14);
    font.setBold(true);
    painter.setFont(font);

    QFontMetrics fm(font);
    int textWidth = fm.horizontalAdvance(infoText) + 20;
    int textHeight = fm.height() + 10;

    QRect textBgRect(rect().right() - textWidth - 10, rect().bottom() - textHeight - 10, textWidth, textHeight);
    painter.fillRect(textBgRect, QColor(0, 0, 0, 160));
    painter.drawText(textBgRect, Qt::AlignCenter, infoText);
}
