#include "MainWidget.h"
#include <QtWidgets>
#include <QApplication>
#include "component/qxtglobalshortcut/qxtglobalshortcut.h"
#include "common/Util.h"
#include "controller/WindowManager.h"

MainWidget::MainWidget(QWidget *parent)
	: QFrame(parent)
{
	tray_ = new SystemTray(this);
	connect(tray_, &SystemTray::sigReload, this, &MainWidget::slotReload);
	connect(tray_, &QSystemTrayIcon::activated, this, &MainWidget::slotTrayActivated);
	tray_->show();

	mainShortcut_ = new QxtGlobalShortcut(this);
	connect(mainShortcut_, &QxtGlobalShortcut::activated, this, &MainWidget::slotMainShortcut);
    setScreenshotGlobalKey(WindowManager::instance()->setting()->screenhotGlobalKey());

    pinShortcut_ = new QxtGlobalShortcut(this);
    connect(pinShortcut_, &QxtGlobalShortcut::activated, this, &MainWidget::slotPinShortcut);
    setPinGlobalKey(WindowManager::instance()->setting()->pinGlobalKey());

    QxtGlobalShortcut *exitShortcut = new QxtGlobalShortcut(this);
    exitShortcut->setShortcut(QKeySequence(Qt::ALT + Qt::Key_Q));
    connect(exitShortcut, &QxtGlobalShortcut::activated, this, &MainWidget::slotExit);

	QVBoxLayout *mLayout = new QVBoxLayout(this);
	mLayout->setContentsMargins(10, 10, 10, 10);
	mLayout->setSpacing(10);
}

MainWidget::~MainWidget()
{
	tray_->hide();
	tray_->deleteLater();
}

bool MainWidget::setScreenshotGlobalKey(const QString &key)
{
    return mainShortcut_->setShortcut(QKeySequence::fromString(key, QKeySequence::NativeText));
}

bool MainWidget::setPinGlobalKey(const QString &key)
{
    return pinShortcut_->setShortcut(QKeySequence::fromString(key, QKeySequence::NativeText));
}

void MainWidget::slotReload()
{
}

void MainWidget::slotTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
	if (reason == QSystemTrayIcon::Trigger) {
		slotMainShortcut();
	}
}

void MainWidget::slotMainShortcut()
{
	WindowManager::instance()->openWidget(WidgetID::SCREENSHOT);
}

void MainWidget::slotPinShortcut()
{
    emit WindowManager::instance()->sigPin();
}

void MainWidget::slotExit()
{
    qApp->exit();
}
