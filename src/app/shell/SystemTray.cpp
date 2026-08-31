#include "SystemTray.h"
#include <QtWidgets>
#include "app/WindowManager.h"
#include "core/theme/MenuCheckMark.h"
#include "modules/clipboard/ClipboardLiteManager.h"

SystemTray::SystemTray(WindowManager* windowManager, ClipboardLiteManager* clipboard, QWidget *parent)
    : QSystemTrayIcon(parent)
    , clipboard_(clipboard)
    , windowManager_(windowManager)
{
    this->setIcon(QIcon(":/images/ntscreenshot.ico"));
    menu_ = new QMenu(parent);
    screenshotAction_ = menu_->addAction(QStringLiteral("截屏"));
    connect(screenshotAction_, &QAction::triggered, this, &SystemTray::onScreenshot);

    pinAction_ = menu_->addAction(QStringLiteral("贴图"));
    connect(pinAction_, &QAction::triggered, this, &SystemTray::onPin);

    textSelectionAction_ = menu_->addAction(QStringLiteral("划词工具"));
    connect(textSelectionAction_, &QAction::triggered, this, &SystemTray::onTextSelection);

    chatAction_ = menu_->addAction(QStringLiteral("对话窗口"));
    connect(chatAction_, &QAction::triggered, this, &SystemTray::onChatAction);

    localSearchAction_ = menu_->addAction(QStringLiteral("本地快速搜索"));
    connect(localSearchAction_, &QAction::triggered, this, &SystemTray::onLocalSearch);

    clipboardMenu_ = menu_->addMenu(QStringLiteral("剪切板"));
    clipboardAction_ = clipboardMenu_->menuAction();
    connect(clipboardMenu_, &QMenu::aboutToShow, this, &SystemTray::onClipboardMenuAboutToShow);
    if (clipboard_) {
        connect(clipboard_, &ClipboardLiteManager::wakeHotkeyChanged,
                this, [this](const QString &hotkey) {
                clipboardAction_->setText(hotkey.isEmpty()
                                              ? QStringLiteral("剪切板")
                                              : QStringLiteral("剪切板 %1").arg(hotkey));
                });
    }

    QAction *settingAction = menu_->addAction(QStringLiteral("设置"));
    connect(settingAction, &QAction::triggered, this, &SystemTray::onSetting);

    menu_->addSeparator();

    QAction *exitAction = menu_->addAction(QStringLiteral("退出"));
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

void SystemTray::onTextSelection()
{
    const bool enabled = !windowManager_->setting()->textSelectionEnabled();
    windowManager_->setting()->setTextSelectionEnabled(enabled);
    emit windowManager_->sigSettingChanged();
}

void SystemTray::onPin()
{
    windowManager_->showAllSticker();
}

void SystemTray::onChatAction()
{
    windowManager_->showLlmChatWindow(QStringLiteral("AI 对话"));
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
    const QString chatKey = windowManager_->setting()->chatGlobalKey();
    QStringList tips;
    tips << "ntscreenshot";
    tips << QStringLiteral("版本v1.0.0");
    tips << QStringLiteral("截图快捷键：%1").arg(windowManager_->setting()->screenhotGlobalKey());
    tips << QStringLiteral("贴图快捷键：%1").arg(windowManager_->setting()->pinGlobalKey());
    tips << QStringLiteral("贴图数目：%1").arg(windowManager_->allStickerCount());
    if (!chatKey.isEmpty()) {
        tips << QStringLiteral("对话窗口快捷键：%1").arg(chatKey);
    }
    this->setToolTip(tips.join("\r\n"));

    screenshotAction_->setText(QStringLiteral("截屏 %1").arg(windowManager_->setting()->screenhotGlobalKey()));
    textSelectionAction_->setText(QStringLiteral("划词工具"));
    MenuCheckMark::apply(textSelectionAction_, windowManager_->setting()->textSelectionEnabled());
    pinAction_->setText(QStringLiteral("贴图 %1").arg(windowManager_->setting()->pinGlobalKey()));
    chatAction_->setText(chatKey.isEmpty()
                             ? QStringLiteral("对话窗口")
                             : QStringLiteral("对话窗口 %1").arg(chatKey));
    const QString searchKey = windowManager_->setting()->localSearchGlobalKey();
    localSearchAction_->setText(searchKey.isEmpty()
                                    ? QStringLiteral("本地快速搜索")
                                    : QStringLiteral("本地快速搜索 %1").arg(searchKey));
    const QString clipboardKey = clipboard_ ? clipboard_->ActiveShowHotkeyLabel() : QString();
    clipboardAction_->setText(clipboardKey.isEmpty()
                                  ? QStringLiteral("剪切板")
                                  : QStringLiteral("剪切板 %1").arg(clipboardKey));
}
