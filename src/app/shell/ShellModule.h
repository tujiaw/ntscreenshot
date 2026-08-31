#pragma once

#include "core/modules/IToolModule.h"

#include <QPointer>

class ClipboardLiteManager;
class FramelessWidget;
class MainWidget;
class WindowManager;

class ShellModule final : public IToolModule {
public:
    ShellModule(WindowManager* windowManager, ClipboardLiteManager* clipboard);

    QString id() const override { return QStringLiteral("shell"); }
    void initialize() override;
    void shutdown() override;

    void open();
    bool setScreenshotGlobalKey(const QString& key);
    bool setPinGlobalKey(const QString& key);
    bool setChatGlobalKey(const QString& key);
    bool setLocalSearchGlobalKey(const QString& key);
    QString lastHotkeyError() const;

private:
    WindowManager* windowManager_ = nullptr;
    ClipboardLiteManager* clipboard_ = nullptr;
    QPointer<FramelessWidget> frame_;
    QPointer<MainWidget> mainWidget_;
};
