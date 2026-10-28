#include "modules/capture/CaptureModule.h"

#include "core/platform/Util.h"
#include "core/settings/SettingModel.h"
#include "modules/capture/gif_recorder/GifRecorderWidget.h"
#include "modules/capture/long_screenshot/LongScreenshotWidget.h"
#include "modules/capture/pin/PinWidget.h"
#include "modules/capture/screenshot/Screenshot.h"
#include "app/WindowManager.h"
#include "shared/ui/TipsWidget.h"

#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImageWriter>
#include <QSaveFile>

CaptureModule::CaptureModule(WindowManager* windowManager)
    : windowManager_(windowManager)
    , settings_(windowManager ? windowManager->setting() : nullptr)
{
    qInfo() << "CaptureModule: created";
}

CaptureModule::~CaptureModule() = default;

void CaptureModule::initialize()
{
    qInfo() << "CaptureModule::initialize";
}

void CaptureModule::shutdown()
{
    qInfo() << "CaptureModule::shutdown";
    if (screenshot_) {
        screenshot_->close();
        screenshot_.reset();
    }
    if (longScreenshot_) {
        longScreenshot_->close();
        longScreenshot_.reset();
    }
    if (gifRecorder_) {
        gifRecorder_->close();
        gifRecorder_.reset();
    }
}

void CaptureModule::openScreenshot()
{
    qInfo() << "CaptureModule::openScreenshot";
    if (screenshot_) {
        if (screenshot_->isVisible()) {
            screenshot_->raise();
            screenshot_->activateWindow();
            screenshot_->setFocus(Qt::ShortcutFocusReason);
            return;
        }
        screenshot_.reset();
    }

    screenshot_ = std::make_unique<ScreenshotWidget>(windowManager_);
    if (settings_) {
        screenshot_->setPinGlobalKey(settings_->pinGlobalKey());
        screenshot_->setRgbColor(settings_->rgbColor());
        screenshot_->setBackgroundColorAlpha(settings_->backgroundColorAlpha());
    }
    connect(screenshot_.get(), &ScreenshotWidget::sigReopen,
            this, &CaptureModule::openScreenshot, Qt::QueuedConnection);
    connect(screenshot_.get(), &ScreenshotWidget::sigClose,
            this, &CaptureModule::closeScreenshot, Qt::QueuedConnection);
    connect(screenshot_.get(), &ScreenshotWidget::sigSaveScreenshot,
            this, &CaptureModule::saveScreenshot, Qt::QueuedConnection);
}

void CaptureModule::openLongScreenshot(const QRect& captureRect, std::shared_ptr<QPixmap> originScreen)
{
    if (auto* previous = longScreenshot_.release()) {
        previous->hide();
        previous->deleteLater();
    }

    longScreenshot_ = std::make_unique<LongScreenshotWidget>(windowManager_, captureRect, originScreen, nullptr);
    connect(longScreenshot_.get(), &LongScreenshotWidget::sigScreenshotFinished,
            this, &CaptureModule::saveScreenshot, Qt::QueuedConnection);
    connect(longScreenshot_.get(), &LongScreenshotWidget::sigWindowClosed, this, [this]() {
        if (longScreenshot_) {
            longScreenshot_.release()->deleteLater();
        }
    }, Qt::QueuedConnection);
}

void CaptureModule::openGifRecorder(const QRect& captureRect)
{
    if (auto* previous = gifRecorder_.release()) {
        previous->hide();
        previous->deleteLater();
    }

    QString outputDirectory;
    if (settings_) {
        bool autoSave = false;
        settings_->getAutoSaveImage(autoSave, outputDirectory);
    }
    gifRecorder_ = std::make_unique<GifRecorderWidget>(captureRect, outputDirectory, nullptr);
    connect(gifRecorder_.get(), &GifRecorderWidget::sigWindowClosed, this, [this]() {
        if (gifRecorder_) {
            gifRecorder_.release()->deleteLater();
        }
    }, Qt::QueuedConnection);
}

void CaptureModule::closeScreenshot()
{
    if (screenshot_) {
        // 立即隐藏，但把全屏位图和子控件的销毁推迟到事件循环空闲时，
        // 避免关闭动作和剪贴板写入一起阻塞 GUI 线程。
        screenshot_->hide();
        screenshot_.release()->deleteLater();
    }
}

void CaptureModule::closeLongScreenshot()
{
    if (longScreenshot_) {
        longScreenshot_->close();
        longScreenshot_.reset();
    }
}

void CaptureModule::togglePin()
{
    if (gifRecorder_) {
        gifRecorder_->stopRecording();
        return;
    }
    if (longScreenshot_) {
        longScreenshot_->stopCapture();
        return;
    }
    if (screenshot_) {
        screenshot_->pin();
        closeScreenshot();
        return;
    }
    if (PinWidget::allCount() == PinWidget::visibleCount()) {
        PinWidget::hideAll();
    } else {
        PinWidget::showAll(windowManager_);
    }
}

void CaptureModule::showAllStickers()
{
    PinWidget::showAll(windowManager_);
}

int CaptureModule::stickerCount() const
{
    return PinWidget::allCount();
}

void CaptureModule::saveScreenshot(const QPixmap& pixmap) const
{
    if (!settings_ || pixmap.isNull()) {
        return;
    }
    bool autoSave = false;
    QString directory;
    settings_->getAutoSaveImage(autoSave, directory);
    if (!autoSave) {
        return;
    }

    auto reportFailure = [](const QString& path, const QString& reason) {
        qWarning() << "Screenshot auto-save failed:" << path << reason;
        const QString message =
            QCoreApplication::translate("App", "截图自动保存失败：%1\n%2").arg(path, reason);
        // popup(nullptr) adopts the active screenshot window, which is about
        // to close. Keep this notification independent of that window.
        auto* tip = new TipsWidget(nullptr);
        tip->setText(message.simplified());
        tip->setToolTip(message);
        tip->autoClose(5);
        tip->show();
    };
    if (directory.trimmed().isEmpty() || !QDir().mkpath(directory)) {
        reportFailure(directory, QCoreApplication::translate("App", "无法创建保存目录"));
        return;
    }

    const QDir outputDir(directory);
    const QString baseName = QFileInfo(Util::pixmapName()).completeBaseName();
    QString filePath = outputDir.filePath(baseName + QStringLiteral(".png"));
    for (int suffix = 1; QFileInfo::exists(filePath); ++suffix) {
        filePath = outputDir.filePath(baseName + QStringLiteral("_%1.png").arg(suffix));
    }

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        reportFailure(filePath, file.errorString());
        return;
    }
    QImageWriter writer(&file, "PNG");
    if (!writer.write(pixmap.toImage())) {
        file.cancelWriting();
        reportFailure(filePath, writer.errorString());
        return;
    }
    if (!file.commit()) {
        reportFailure(filePath, file.errorString());
    }
}
