#pragma once

#include "core/modules/IToolModule.h"

#include <QObject>
#include <QPixmap>

#include <memory>

class GifRecorderWidget;
class LongScreenshotWidget;
class ScreenshotWidget;
class SettingModel;
class WindowManager;

class CaptureModule final : public QObject, public IToolModule {
    Q_OBJECT

public:
    explicit CaptureModule(WindowManager* windowManager);
    ~CaptureModule() override;

    QString id() const override { return QStringLiteral("capture"); }
    void initialize() override;
    void shutdown() override;

    void openScreenshot();
    void openLongScreenshot(const QRect& captureRect, std::shared_ptr<QPixmap> originScreen);
    void openGifRecorder(const QRect& captureRect);
    void closeScreenshot();
    void closeLongScreenshot();
    void togglePin();
    void showAllStickers();
    int stickerCount() const;

private:
    void saveScreenshot(const QPixmap& pixmap) const;

    WindowManager* windowManager_ = nullptr;
    SettingModel* settings_ = nullptr;
    std::unique_ptr<ScreenshotWidget> screenshot_;
    std::unique_ptr<LongScreenshotWidget> longScreenshot_;
    std::unique_ptr<GifRecorderWidget> gifRecorder_;
};
