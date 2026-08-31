#include "modules/capture/CaptureModule.h"

#include "core/platform/Util.h"
#include "core/settings/SettingModel.h"
#include "modules/capture/gif_recorder/GifRecorderWidget.h"
#include "modules/capture/long_screenshot/LongScreenshotWidget.h"
#include "modules/capture/pin/PinWidget.h"
#include "modules/capture/screenshot/Screenshot.h"
#include "app/WindowManager.h"

#include <QDebug>

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

    gifRecorder_ = std::make_unique<GifRecorderWidget>(captureRect, nullptr);
    connect(gifRecorder_.get(), &GifRecorderWidget::sigWindowClosed, this, [this]() {
        if (gifRecorder_) {
            gifRecorder_.release()->deleteLater();
        }
    }, Qt::QueuedConnection);
}

void CaptureModule::closeScreenshot()
{
    if (screenshot_) {
        screenshot_->close();
        screenshot_.reset();
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
    if (!settings_) {
        return;
    }
    bool autoSave = false;
    QString directory;
    settings_->getAutoSaveImage(autoSave, directory);
    if (autoSave) {
        pixmap.save(directory + "/" + Util::pixmapName());
    }
}
