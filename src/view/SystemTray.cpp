#include "SystemTray.h"
#include <QtWidgets>
#include "controller/WindowManager.h"

SystemTray::SystemTray(QWidget *parent)
	: QSystemTrayIcon(parent)
{
	this->setIcon(QIcon(":/images/ntscreenshot.ico"));
	menu_ = new QMenu(parent);
	screenshotAction_ = menu_->addAction(QStringLiteral("����"));
    connect(screenshotAction_, &QAction::triggered, this, &SystemTray::onScreenshot);
    
    pinAction_ = menu_->addAction(QStringLiteral("��ͼ"));
    connect(pinAction_, &QAction::triggered, this, &SystemTray::onPin);

	QAction *settingAction = menu_->addAction(QStringLiteral("����"));
    connect(settingAction, &QAction::triggered, this, &SystemTray::onSetting);

    menu_->addSeparator();

	QAction *exitAction = menu_->addAction(QStringLiteral("�˳�"));
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
    tips << QStringLiteral("�汾v1.0.0");
    tips << QStringLiteral("��ͼ��ݼ���%1").arg(WindowManager::instance()->setting()->screenhotGlobalKey());
    tips << QStringLiteral("��ͼ��ݼ���%1").arg(WindowManager::instance()->setting()->pinGlobalKey());
    tips << QStringLiteral("��ͼ��Ŀ��%1").arg(WindowManager::instance()->allStickerCount());
    this->setToolTip(tips.join("\r\n"));

    screenshotAction_->setText(QStringLiteral("���� %1").arg(WindowManager::instance()->setting()->screenhotGlobalKey()));
    pinAction_->setText(QStringLiteral("��ͼ %1").arg(WindowManager::instance()->setting()->pinGlobalKey()));
}
