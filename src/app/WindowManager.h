#pragma once

#include "core/settings/SettingModel.h"

#include <QObject>
#include <QPixmap>

#include <memory>

class ModuleRegistry;

class WindowManager : public QObject {
    Q_OBJECT

public:
    WindowManager();
    ~WindowManager() override;

    void setModuleRegistry(ModuleRegistry* modules);
    void destroy();
    void openWidget(const QString& id);
    void openLongScreenshotWidget(const QRect& captureRect, std::shared_ptr<QPixmap> originScreen);
    void openGifRecorderWidget(const QRect& captureRect);

    SettingModel* setting() { return settingModel_.get(); }
    bool setScreenshotGlobalKey(const QString& key);
    bool setPinGlobalKey(const QString& key);
    bool setChatGlobalKey(const QString& key);
    bool setLocalSearchGlobalKey(const QString& key);
    QString lastHotkeyError() const;
    void rebuildLocalSearchIndex();
    QString localSearchStatus() const;
    void showAllSticker();
    int allStickerCount();
    void showLlmChatWindow(const QString& title, const QString& text = {}, const QPixmap& pixmap = {});

signals:
    void sigPin();
    void sigSettingChanged();
    void sigStickerCountChanged();

private:
    void onTextSelectionActionTriggered(const QString& actionId,
                                        const QString& selectedText,
                                        const QString& inputText);

    ModuleRegistry* modules_ = nullptr;
    std::unique_ptr<SettingModel> settingModel_;
};
