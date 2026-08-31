#include "MainWidget.h"

#include "app/shell/GlobalHotkeyRegistry.h"

#include <QtWidgets>
#include <QApplication>

#include "core/platform/Util.h"
#include "app/WindowManager.h"

namespace {
const QString kScreenshotHotkey = QStringLiteral("screenshot");
const QString kPinHotkey = QStringLiteral("pin");
const QString kChatHotkey = QStringLiteral("chat");
const QString kLocalSearchHotkey = QStringLiteral("local_search");
const QString kExitHotkey = QStringLiteral("exit");
const QString kDefaultLocalSearchHotkey = QStringLiteral("Alt+Space");
const QString kFallbackLocalSearchHotkey = QStringLiteral("Ctrl+Alt+Space");
}

MainWidget::MainWidget(WindowManager* windowManager, ClipboardLiteManager* clipboard, QWidget *parent)
    : QFrame(parent)
    , windowManager_(windowManager)
    , hotkeys_(std::make_unique<GlobalHotkeyRegistry>(this))
{
    tray_ = new SystemTray(windowManager_, clipboard, this);
    connect(tray_, &QSystemTrayIcon::activated, this, &MainWidget::slotTrayActivated);
    tray_->show();

    setScreenshotGlobalKey(windowManager_->setting()->screenhotGlobalKey());
    setPinGlobalKey(windowManager_->setting()->pinGlobalKey());
    setChatGlobalKey(windowManager_->setting()->chatGlobalKey());
    const QString localSearchKey = windowManager_->setting()->localSearchGlobalKey();
    qInfo() << "MainWidget: registering local search hotkey:" << localSearchKey;
    if (!setLocalSearchGlobalKey(localSearchKey)) {
        qWarning() << "MainWidget:" << localSearchKey
                   << "registration failed (likely occupied by another app)";
        const bool usesDefault = localSearchKey.compare(
            kDefaultLocalSearchHotkey, Qt::CaseInsensitive) == 0;
        if (usesDefault && setLocalSearchGlobalKey(kFallbackLocalSearchHotkey)) {
            qInfo() << "MainWidget: fallback hotkey registered:" << kFallbackLocalSearchHotkey;
            windowManager_->setting()->setLocalSearchGlobalKey(kFallbackLocalSearchHotkey);
            emit windowManager_->sigSettingChanged();
        } else {
            qWarning() << "MainWidget: failed to register fallback hotkey too:" << localSearchKey;
        }
    } else {
        qInfo() << "MainWidget: local search hotkey registered successfully:" << localSearchKey;
    }

    hotkeys_->setHotkey(kExitHotkey, QStringLiteral("Alt+Q"), [this]() { slotExit(); });

    QVBoxLayout *mLayout = new QVBoxLayout(this);
    mLayout->setContentsMargins(10, 10, 10, 10);
    mLayout->setSpacing(10);
}

MainWidget::~MainWidget()
{
    if (tray_) {
        tray_->hide();
    }
}

bool MainWidget::setScreenshotGlobalKey(const QString &key)
{
    return hotkeys_->setHotkey(kScreenshotHotkey, key, [this]() { slotMainShortcut(); });
}

bool MainWidget::setPinGlobalKey(const QString &key)
{
    return hotkeys_->setHotkey(kPinHotkey, key, [this]() { slotPinShortcut(); });
}

bool MainWidget::setChatGlobalKey(const QString &key)
{
    return hotkeys_->setHotkey(kChatHotkey, key, [this]() { slotChatShortcut(); });
}

bool MainWidget::setLocalSearchGlobalKey(const QString& key)
{
    return hotkeys_->setHotkey(kLocalSearchHotkey, key,
                               [this]() { slotLocalSearchShortcut(); });
}

QString MainWidget::lastHotkeyError() const
{
    return hotkeys_ ? hotkeys_->lastError() : QString();
}

void MainWidget::slotTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        slotMainShortcut();
    }
}

void MainWidget::slotMainShortcut()
{
    windowManager_->openWidget(WidgetID::SCREENSHOT);
}

void MainWidget::slotPinShortcut()
{
    emit windowManager_->sigPin();
}

void MainWidget::slotChatShortcut()
{
    windowManager_->showLlmChatWindow(QStringLiteral("AI 对话"));
}

void MainWidget::slotLocalSearchShortcut()
{
    windowManager_->openWidget(WidgetID::LOCAL_SEARCH);
}

void MainWidget::slotExit()
{
    qApp->exit();
}
