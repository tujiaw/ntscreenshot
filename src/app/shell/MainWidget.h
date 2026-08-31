#pragma once

#include <QFrame>
#include <memory>
#include "SystemTray.h"

class GlobalHotkeyRegistry;
class ClipboardLiteManager;
class LnkListView;
class QLineEdit;
class WindowManager;
class MainWidget : public QFrame
{
	Q_OBJECT

public:
	MainWidget(WindowManager* windowManager, ClipboardLiteManager* clipboard, QWidget *parent);
	~MainWidget();

    bool setScreenshotGlobalKey(const QString &key);
    bool setPinGlobalKey(const QString &key);
    bool setChatGlobalKey(const QString &key);
    bool setLocalSearchGlobalKey(const QString& key);
    QString lastHotkeyError() const;
    SystemTray* systemTray() const { return tray_; }

private slots:
	void slotTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void slotMainShortcut();
    void slotPinShortcut();
    void slotChatShortcut();
    void slotLocalSearchShortcut();
    void slotExit();

private:
	Q_DISABLE_COPY(MainWidget)
	SystemTray *tray_;
    WindowManager* windowManager_ = nullptr;
    std::unique_ptr<GlobalHotkeyRegistry> hotkeys_;
};
