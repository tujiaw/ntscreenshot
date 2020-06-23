#include "SystemTray.h"
#include <QtWidgets>
#include "controller/WindowManager.h"

SystemTray::SystemTray(QWidget *parent)
	: QSystemTrayIcon(parent)
{
    SettingModel *setting = WindowManager::instance()->setting();

	this->setIcon(QIcon(":/images/ntscreenshot.ico"));
	menu_ = new QMenu(parent);
	screenshotAction_ = menu_->addAction(QStringLiteral("���� ") + setting->screenhotGlobalKey());
    connect(screenshotAction_, &QAction::triggered, this, &SystemTray::onScreenshot);
    
    pinAction_ = menu_->addAction(QStringLiteral("��ͼ ") + setting->pinKey());
    connect(pinAction_, &QAction::triggered, this, &SystemTray::onPin);

	QAction *settingAction = menu_->addAction(QStringLiteral("����"));
    connect(settingAction, &QAction::triggered, this, &SystemTray::onSetting);

    menu_->addSeparator();

	QAction *exitAction = menu_->addAction(QStringLiteral("�˳�"));
    connect(exitAction, &QAction::triggered, this, &SystemTray::onExit);

	this->setContextMenu(menu_);

    connect(WindowManager::instance(), &WindowManager::sigSettingChanged, this, &SystemTray::onSettingChanged);
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

}

void SystemTray::onSetting()
{
    WindowManager::instance()->openWidget(WidgetID::SETTINGS);
}

void SystemTray::onExit()
{
    qApp->exit();
}

void SystemTray::onSettingChanged()
{
    SettingModel *setting = WindowManager::instance()->setting();
    screenshotAction_->setText(QStringLiteral("���� ") + setting->screenhotGlobalKey());
    pinAction_->setText(QStringLiteral("��ͼ ") + setting->pinKey());
}
