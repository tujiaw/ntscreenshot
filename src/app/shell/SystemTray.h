#pragma once

#include <QSystemTrayIcon>

class QAction;
class QMenu;
class ClipboardLiteManager;
class WindowManager;
class SystemTray : public QSystemTrayIcon
{
	Q_OBJECT

public:
	SystemTray(WindowManager* windowManager, ClipboardLiteManager* clipboard, QWidget *parent);
	~SystemTray();

private slots:
    void onScreenshot();
    void onTextSelection();
    void onPin();
    void onChatAction();
    void onLocalSearch();
    void onSetting();
    void onExit();
    void onUpdate();
    void onClipboardMenuAboutToShow();

private:
    Q_DISABLE_COPY(SystemTray)
	QWidget *parent_;
    QAction *screenshotAction_;
    QAction *textSelectionAction_;
    QAction *pinAction_;
    QAction *chatAction_;
	QAction *localSearchAction_;
	QAction *clipboardAction_;
	QMenu *menu_;
    QMenu *clipboardMenu_;
    ClipboardLiteManager* clipboard_;
    WindowManager* windowManager_ = nullptr;
};
