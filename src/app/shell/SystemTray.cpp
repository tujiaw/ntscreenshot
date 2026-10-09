#include <QCoreApplication>
#include "SystemTray.h"
#include <QtWidgets>
#include "app/WindowManager.h"
#include "modules/clipboard/ClipboardLiteManager.h"

SystemTray::SystemTray(WindowManager* windowManager, ClipboardLiteManager* clipboard, QWidget *parent)
    : QSystemTrayIcon(parent)
    , clipboard_(clipboard)
    , windowManager_(windowManager)
{
    this->setIcon(QIcon(":/images/ntscreenshot.ico"));
    menu_ = new QMenu(parent);
    screenshotAction_ = menu_->addAction(QCoreApplication::translate("App", "截屏"));
    connect(screenshotAction_, &QAction::triggered, this, &SystemTray::onScreenshot);

    pinAction_ = menu_->addAction(QCoreApplication::translate("App", "贴图"));
    connect(pinAction_, &QAction::triggered, this, &SystemTray::onPin);

    clipboardMenu_ = menu_->addMenu(QCoreApplication::translate("App", "剪切板"));
    clipboardAction_ = clipboardMenu_->menuAction();
    connect(clipboardMenu_, &QMenu::aboutToShow, this, &SystemTray::onClipboardMenuAboutToShow);
    if (clipboard_) {
        connect(clipboard_, &ClipboardLiteManager::wakeHotkeyChanged,
                this, [this](const QString &hotkey) {
                clipboardAction_->setText(hotkey.isEmpty()
                                              ? QCoreApplication::translate("App", "剪切板")
                                              : QCoreApplication::translate("App", "剪切板 %1").arg(hotkey));
                });
    }

    localSearchAction_ = menu_->addAction(QCoreApplication::translate("App", "快捷启动"));
    connect(localSearchAction_, &QAction::triggered, this, &SystemTray::onLocalSearch);

    QAction *settingAction = menu_->addAction(QCoreApplication::translate("App", "设置"));
    connect(settingAction, &QAction::triggered, this, &SystemTray::onSetting);

    menu_->addSeparator();

    QAction *exitAction = menu_->addAction(QCoreApplication::translate("App", "退出"));
    connect(exitAction, &QAction::triggered, this, &SystemTray::onExit);

    this->setContextMenu(menu_);

    connect(windowManager_, &WindowManager::sigSettingChanged, this, &SystemTray::onUpdate, Qt::QueuedConnection);
    connect(windowManager_, &WindowManager::sigStickerCountChanged, this, &SystemTray::onUpdate, Qt::QueuedConnection);
    onUpdate();
}

SystemTray::~SystemTray()
{
}

void SystemTray::onScreenshot()
{
    windowManager_->openWidget(WidgetID::SCREENSHOT);
}

void SystemTray::onPin()
{
    windowManager_->showAllSticker();
}

void SystemTray::onLocalSearch()
{
    windowManager_->openWidget(WidgetID::LOCAL_SEARCH);
}

void SystemTray::onSetting()
{
    windowManager_->openWidget(WidgetID::SETTINGS);
}

void SystemTray::onExit()
{
    qApp->exit();
}

void SystemTray::onClipboardMenuAboutToShow()
{
    if (clipboard_) {
        clipboard_->PopulateClipboardMenu(clipboardMenu_);
    }
}

void SystemTray::onUpdate()
{

    QStringList tips;
    tips << "ntscreenshot";
    tips << QCoreApplication::translate("App", "版本v1.0.0");
    tips << QCoreApplication::translate("App", "截图快捷键：%1").arg(windowManager_->setting()->screenhotGlobalKey());
    tips << QCoreApplication::translate("App", "贴图快捷键：%1").arg(windowManager_->setting()->pinGlobalKey());
    tips << QCoreApplication::translate("App", "贴图数目：%1").arg(windowManager_->allStickerCount());

    this->setToolTip(tips.join("\r\n"));

    screenshotAction_->setText(QCoreApplication::translate("App", "截屏 %1").arg(windowManager_->setting()->screenhotGlobalKey()));

    pinAction_->setText(QCoreApplication::translate("App", "贴图 %1").arg(windowManager_->setting()->pinGlobalKey()));

    const QString searchKey = windowManager_->setting()->localSearchGlobalKey();
    localSearchAction_->setText(searchKey.isEmpty()
                                    ? QCoreApplication::translate("App", "快捷启动")
                                    : QCoreApplication::translate("App", "快捷启动 %1").arg(searchKey));
    const QString clipboardKey = clipboard_ ? clipboard_->ActiveShowHotkeyLabel() : QString();
    clipboardAction_->setText(clipboardKey.isEmpty()
                                  ? QCoreApplication::translate("App", "剪切板")
                                  : QCoreApplication::translate("App", "剪切板 %1").arg(clipboardKey));
}
