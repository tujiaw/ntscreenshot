#pragma once

#include <QSystemTrayIcon>

class QAction;
class SystemTray : public QSystemTrayIcon
{
	Q_OBJECT

public:
	SystemTray(QWidget *parent);
	~SystemTray();

signals:
	void sigReload();

private slots:
    void onScreenshot();
    void onPin();
    void onSetting();
    void onExit();
    void onUpdate();
    void onTest();

private:
    Q_DISABLE_COPY(SystemTray)
	QWidget *parent_;
    QAction *screenshotAction_;
    QAction *pinAction_;
	QMenu *menu_;
};
