#pragma once

#include <QFrame>
#include "SystemTray.h"

class QxtGlobalShortcut;
class LnkListView;
class QLineEdit;
class MainWidget : public QFrame
{
	Q_OBJECT

public:
	MainWidget(QWidget *parent);
	~MainWidget();

    bool setScreenshotGlobalKey(const QString &key);

private slots:
	void slotReload();
	void slotTrayActivated(QSystemTrayIcon::ActivationReason reason);
	void slotMainShortcut();
    void slotExit();

private:
	SystemTray *tray_;
	QxtGlobalShortcut *mainShortcut_;
};
