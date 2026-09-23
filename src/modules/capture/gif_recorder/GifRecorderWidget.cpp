#include "GifRecorderWidget.h"

#include "GifEncoderWorker.h"
#include "GifRecorderControlPanel.h"
#include "core/platform/Util.h"
#include "core/theme/ThemeManager.h"
#include "shared/ui/TipsWidget.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMimeData>
#include <QPainter>
#include <QRegion>
#include <QShortcut>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QUrl>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif
#endif

GifRecorderWidget::GifRecorderWidget(const QRect &captureRect, const QString &outputDirectory,
                                     QWidget *parent)
    : QWidget(parent)
    , captureRect_(Util::clampToDesktopLocal(captureRect))
    , outputDirectory_(outputDirectory)
    , controlPanel_(new GifRecorderControlPanel())
    , captureTimer_(new QTimer(this))
    , encoderThread_(new QThread(this))
    , encoderWorker_(new GifEncoderWorker())
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setGeometry(QRect(captureRect_.topLeft() + Util::desktopRect().topLeft(), captureRect_.size()));
    setFocusPolicy(Qt::StrongFocus);

    const QRegion outer(rect());
    const QRegion inner(rect().adjusted(borderWidth_, borderWidth_, -borderWidth_, -borderWidth_));
    setMask(outer.subtracted(inner));

    controlPanel_->setCaptureSize(contentRect().size());
    connect(controlPanel_, &GifRecorderControlPanel::sigRecordClicked, this, &GifRecorderWidget::onRecord);
    connect(controlPanel_, &GifRecorderControlPanel::sigCopyClicked, this, &GifRecorderWidget::onCopy);
    connect(controlPanel_, &GifRecorderControlPanel::sigOpenClicked, this, &GifRecorderWidget::onOpen);
    connect(controlPanel_, &GifRecorderControlPanel::sigCancelClicked, this, &GifRecorderWidget::onCancel);
    auto *escapeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), controlPanel_);
    escapeShortcut->setContext(Qt::ApplicationShortcut);
    connect(escapeShortcut, &QShortcut::activated, this, &GifRecorderWidget::onCancel);
    connect(captureTimer_, &QTimer::timeout, this, &GifRecorderWidget::onCaptureTimer);

    encoderWorker_->moveToThread(encoderThread_);
    connect(encoderThread_, &QThread::finished, encoderWorker_, &QObject::deleteLater);
    connect(this, &GifRecorderWidget::sigBeginEncoding, encoderWorker_, &GifEncoderWorker::begin, Qt::QueuedConnection);
    connect(this, &GifRecorderWidget::sigEncodeFrame, encoderWorker_, &GifEncoderWorker::addFrame, Qt::QueuedConnection);
    connect(this, &GifRecorderWidget::sigFinishEncoding, encoderWorker_, &GifEncoderWorker::finish, Qt::QueuedConnection);
    connect(this, &GifRecorderWidget::sigCancelEncoding, encoderWorker_, &GifEncoderWorker::cancel, Qt::QueuedConnection);
    connect(encoderWorker_, &GifEncoderWorker::sigReady, this, &GifRecorderWidget::onEncoderReady, Qt::QueuedConnection);
    connect(encoderWorker_, &GifEncoderWorker::sigFrameWritten, this, &GifRecorderWidget::onFrameWritten, Qt::QueuedConnection);
    connect(encoderWorker_, &GifEncoderWorker::sigFinished, this, &GifRecorderWidget::onEncodingFinished, Qt::QueuedConnection);
    encoderThread_->start();

    show();
    raise();
    controlPanel_->show();
#ifdef Q_OS_WIN
    // Make the controls an owned top-level window so they always stay above
    // the capture border without being clipped by its hollow window mask.
    SetWindowLongPtrW(reinterpret_cast<HWND>(controlPanel_->winId()), GWLP_HWNDPARENT,
                      static_cast<LONG_PTR>(winId()));
#endif
    positionControlPanel();
    controlPanel_->raise();

#ifdef Q_OS_WIN
    SetWindowDisplayAffinity(reinterpret_cast<HWND>(winId()), WDA_EXCLUDEFROMCAPTURE);
    SetWindowDisplayAffinity(reinterpret_cast<HWND>(controlPanel_->winId()), WDA_EXCLUDEFROMCAPTURE);
#endif
}

GifRecorderWidget::~GifRecorderWidget()
{
    captureTimer_->stop();
    if (controlPanel_) {
        controlPanel_->close();
        controlPanel_->deleteLater();
        controlPanel_ = nullptr;
    }
    if (encoderThread_->isRunning()) {
        QMetaObject::invokeMethod(encoderWorker_, "cancel", Qt::BlockingQueuedConnection);
        encoderThread_->quit();
        encoderThread_->wait();
    }
}

QRect GifRecorderWidget::contentRect() const
{
    return captureRect_.adjusted(borderWidth_, borderWidth_, -borderWidth_, -borderWidth_);
}

QString GifRecorderWidget::defaultOutputPath() const
{
    QString directory = outputDirectory_.trimmed();
    if (directory.isEmpty()) {
        directory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }
    QDir dir(directory);
    dir.mkpath(QStringLiteral("."));

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    return dir.absoluteFilePath(QStringLiteral("recording_%1.gif").arg(timestamp));
}

void GifRecorderWidget::positionControlPanel()
{
    controlPanel_->adjustSize();
    const QRect globalCapture(captureRect_.topLeft() + Util::desktopRect().topLeft(), captureRect_.size());
    QRect visibleBounds = Util::desktopRect();

    QSize panelSize = controlPanel_->size();
#ifdef Q_OS_WIN
    RECT nativeRect = {};
    const HWND panelWindow = reinterpret_cast<HWND>(controlPanel_->winId());
    if (GetWindowRect(panelWindow, &nativeRect)) {
        panelSize = QSize(nativeRect.right - nativeRect.left,
                          nativeRect.bottom - nativeRect.top);
    }

    // Clamp against the physical monitor containing the capture center, not
    // the whole virtual desktop. Otherwise a valid virtual-desktop position
    // can still place the controls on the neighboring display.
    RECT captureNativeRect = {globalCapture.left(), globalCapture.top(),
                              globalCapture.right() + 1, globalCapture.bottom() + 1};
    const HMONITOR captureMonitor = MonitorFromRect(&captureNativeRect, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo = {};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (captureMonitor && GetMonitorInfoW(captureMonitor, &monitorInfo)) {
        visibleBounds = QRect(monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
                              monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                              monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top);
    }
#endif

    constexpr int gap = 6;
    int x = globalCapture.right() - panelSize.width() + 1;
    int y = 0;
    const int aboveY = globalCapture.top() - panelSize.height() - gap;
    const int belowY = globalCapture.bottom() + gap + 1;
    if (aboveY >= visibleBounds.top()) {
        y = aboveY;
    } else if (belowY + panelSize.height() <= visibleBounds.bottom() + 1) {
        y = belowY;
    } else {
        // Full-screen and near-full-screen captures have no room outside the
        // selection. Keep the controls inside its top-right corner instead.
        y = globalCapture.top() + gap;
    }

    const int maxX = visibleBounds.right() - panelSize.width() + 1;
    const int maxY = visibleBounds.bottom() - panelSize.height() + 1;
    x = maxX >= visibleBounds.left() ? qBound(visibleBounds.left(), x, maxX) : visibleBounds.left();
    y = maxY >= visibleBounds.top() ? qBound(visibleBounds.top(), y, maxY) : visibleBounds.top();

#ifdef Q_OS_WIN
    SetWindowPos(panelWindow, HWND_TOPMOST, x, y, 0, 0,
                 SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
#else
    controlPanel_->move(x, y);
#endif
}

void GifRecorderWidget::onRecord()
{
    if (state_ != State::Preparing || !contentRect().isValid()) {
        return;
    }

    outputPath_ = defaultOutputPath();

    state_ = State::Starting;
    controlPanel_->setStarting();
    emit sigBeginEncoding(outputPath_, contentRect().size());
}

void GifRecorderWidget::onEncoderReady(bool success, const QString &error)
{
    if (state_ != State::Starting) {
        return;
    }
    if (!success) {
        state_ = State::Preparing;
        controlPanel_->setPreparing();
        TipsWidget::popup(nullptr, QStringLiteral("无法开始 GIF 录制\n%1").arg(error), 3, 0, true);
        return;
    }

    pendingFrames_ = 0;
    capturedFrames_ = 0;
    lastQueuedAtMs_ = 0;
    elapsed_.restart();
    state_ = State::Recording;
    controlPanel_->setRecording();
    positionControlPanel();
    update();
    captureTimer_->start(qMax(1, 1000 / controlPanel_->framesPerSecond()));
    onCaptureTimer();
}

void GifRecorderWidget::onCaptureTimer()
{
    if (state_ != State::Recording) {
        return;
    }

    const qint64 now = elapsed_.elapsed();
    controlPanel_->setElapsedMilliseconds(now, capturedFrames_);
    if (pendingFrames_ >= 2) {
        return;
    }

    QImage frame = Util::grabDesktopImage(contentRect());
    if (frame.isNull()) {
        return;
    }
    const int delay = qMax(1, static_cast<int>((now - lastQueuedAtMs_ + 5) / 10));
    lastQueuedAtMs_ = now;
    ++pendingFrames_;
    ++capturedFrames_;
    emit sigEncodeFrame(frame, delay);
}

void GifRecorderWidget::onFrameWritten()
{
    pendingFrames_ = qMax(0, pendingFrames_ - 1);
    if (state_ == State::Encoding && pendingFrames_ == 0) {
        emit sigFinishEncoding();
    }
}

void GifRecorderWidget::onStop()
{
    if (state_ != State::Recording) {
        return;
    }
    captureTimer_->stop();
    state_ = State::Encoding;
    update();
    controlPanel_->setElapsedMilliseconds(elapsed_.elapsed(), capturedFrames_);
    controlPanel_->setEncoding();
    if (pendingFrames_ == 0) {
        emit sigFinishEncoding();
    }
}

void GifRecorderWidget::stopRecording()
{
    onStop();
}

void GifRecorderWidget::onCancel()
{
    if (state_ == State::Closed || state_ == State::Encoding) {
        return;
    }
    captureTimer_->stop();
    emit sigCancelEncoding();
    closeRecorder();
}

void GifRecorderWidget::onCopy()
{
    if (state_ != State::Recording) {
        return;
    }
    completionAction_ = CompletionAction::Copy;
    onStop();
}

void GifRecorderWidget::onOpen()
{
    if (state_ != State::Recording) {
        return;
    }
    completionAction_ = CompletionAction::Open;
    onStop();
}

void GifRecorderWidget::onEncodingFinished(bool success, const QString &path, const QString &error)
{
    if (state_ == State::Closed) {
        return;
    }
    if (success) {
        outputPath_ = path;
        if (completionAction_ == CompletionAction::Copy && QFileInfo::exists(outputPath_)) {
            auto *mimeData = new QMimeData();
            mimeData->setUrls({QUrl::fromLocalFile(outputPath_)});
            mimeData->setText(QDir::toNativeSeparators(outputPath_));
            QApplication::clipboard()->setMimeData(mimeData);
        } else if (completionAction_ == CompletionAction::Open && QFileInfo::exists(outputPath_)) {
            Util::shellExecute(outputPath_);
        }
        closeRecorder();
    } else {
        TipsWidget::popup(nullptr, QStringLiteral("GIF 保存失败\n%1").arg(error), 4, 0, true);
        closeRecorder();
    }
}

void GifRecorderWidget::closeRecorder()
{
    if (state_ == State::Closed) {
        return;
    }
    state_ = State::Closed;
    captureTimer_->stop();
    if (controlPanel_) {
        controlPanel_->hide();
    }
    hide();
    emit sigWindowClosed();
}

void GifRecorderWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        onCancel();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void GifRecorderWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    const ThemeTokens& tokens = ThemeManager::tokens();
    QPen pen(state_ == State::Recording ? tokens.danger : tokens.accent, borderWidth_);
    pen.setStyle(Qt::DashLine);
    pen.setDashPattern({4, 2});
    painter.setPen(pen);
    painter.drawRect(rect().adjusted(1, 1, -1, -1));
}
