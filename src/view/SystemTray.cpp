#include "SystemTray.h"
#include <QtWidgets>
#include "controller/WindowManager.h"

SystemTray::SystemTray(QWidget *parent)
	: QSystemTrayIcon(parent)
{
	this->setIcon(QIcon(":/images/ntscreenshot.ico"));
	menu_ = new QMenu(parent);
	screenshotAction_ = menu_->addAction(QStringLiteral("½ØÆÁ"));
    connect(screenshotAction_, &QAction::triggered, this, &SystemTray::onScreenshot);
    
    pinAction_ = menu_->addAction(QStringLiteral("ÌùÍ¼"));
    connect(pinAction_, &QAction::triggered, this, &SystemTray::onPin);

	QAction *settingAction = menu_->addAction(QStringLiteral("ÉèÖÃ"));
    connect(settingAction, &QAction::triggered, this, &SystemTray::onSetting);

    menu_->addSeparator();

	QAction *exitAction = menu_->addAction(QStringLiteral("ÍË³ö"));
    connect(exitAction, &QAction::triggered, this, &SystemTray::onExit);

	this->setContextMenu(menu_);

    onUpdate();
    connect(WindowManager::instance(), &WindowManager::sigSettingChanged, this, &SystemTray::onUpdate, Qt::QueuedConnection);
    connect(WindowManager::instance(), &WindowManager::sigStickerCountChanged, this, &SystemTray::onUpdate, Qt::QueuedConnection);
}

SystemTray::~SystemTray()
{
}

void SystemTray::onScreenshot()
{
    WindowManager::instance()->openWidget(WidgetID::SCREENSHOT);
}

void SystemTray::onPin()
{
    WindowManager::instance()->showAllSticker();
}

void SystemTray::onSetting()
{
    WindowManager::instance()->openWidget(WidgetID::SETTINGS);
}

void SystemTray::onExit()
{
    qApp->exit();
}

void SystemTray::onUpdate()
{
    QStringList tips;
    tips << "ntscreenshot";
    tips << QStringLiteral("°æ±¾v1.0.0");
    tips << QStringLiteral("½ØÍ¼¿ì½Ý¼ü£º%1").arg(WindowManager::instance()->setting()->screenhotGlobalKey());
    tips << QStringLiteral("ÌùÍ¼¿ì½Ý¼ü£º%1").arg(WindowManager::instance()->setting()->pinGlobalKey());
    tips << QStringLiteral("ÌùÍ¼ÊýÄ¿£º%1").arg(WindowManager::instance()->allStickerCount());
    this->setToolTip(tips.join("\r\n"));

    screenshotAction_->setText(QStringLiteral("½ØÆÁ %1").arg(WindowManager::instance()->setting()->screenhotGlobalKey()));
    pinAction_->setText(QStringLiteral("ÌùÍ¼ %1").arg(WindowManager::instance()->setting()->pinGlobalKey()));
}
