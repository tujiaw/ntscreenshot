#include "app/WindowManager.h"

#include "app/ModuleRegistry.h"
#include "app/shell/ShellModule.h"

#include <QDebug>
#include "core/foundation/Constants.h"
#include "modules/assistant/AssistantModule.h"
#include "modules/capture/CaptureModule.h"
#include "modules/settings/SettingsModule.h"
#include "modules/text_selection/TextSelectionModule.h"
#include "modules/local_search/LocalSearchModule.h"

WindowManager::WindowManager()
    : settingModel_(std::make_unique<SettingModel>(nullptr))
{
    qInfo() << "WindowManager: constructing...";
    connect(this, &WindowManager::sigPin, this, [this]() {
        if (modules_) {
            if (auto* capture = modules_->module<CaptureModule>()) {
                capture->togglePin();
            }
        }
    });
    qInfo() << "WindowManager: constructed";
}

WindowManager::~WindowManager()
{
    qInfo() << "WindowManager: destructing...";
    destroy();
    qInfo() << "WindowManager: destructed";
}

void WindowManager::setModuleRegistry(ModuleRegistry* modules)
{
    modules_ = modules;
    if (modules_) {
        if (auto* textSelection = modules_->module<TextSelectionModule>()) {
            connect(textSelection, &TextSelectionModule::actionTriggered,
                    this, &WindowManager::onTextSelectionActionTriggered,
                    Qt::UniqueConnection);
        }
    }
}

void WindowManager::destroy()
{
    if (modules_) {
        modules_->shutdownAll();
        modules_ = nullptr;
    }
}

void WindowManager::openWidget(const QString& id)
{
    qInfo() << "WindowManager::openWidget:" << id;
    if (!modules_) {
        qWarning() << "WindowManager::openWidget: no module registry";
        return;
    }
    if (id == WidgetID::SCREENSHOT) {
        if (auto* capture = modules_->module<CaptureModule>()) {
            capture->openScreenshot();
        }
    } else if (id == WidgetID::MAIN) {
        if (auto* shell = modules_->module<ShellModule>()) {
            shell->open();
        }
    } else if (id == WidgetID::SETTINGS) {
        if (auto* settings = modules_->module<SettingsModule>()) {
            settings->open();
        }
    } else if (id == WidgetID::LOCAL_SEARCH) {
        if (auto* localSearch = modules_->module<LocalSearchModule>()) {
            localSearch->open();
        }
    }
}

void WindowManager::openLongScreenshotWidget(const QRect& captureRect,
                                             std::shared_ptr<QPixmap> originScreen)
{
    if (modules_) {
        if (auto* capture = modules_->module<CaptureModule>()) {
            capture->openLongScreenshot(captureRect, std::move(originScreen));
        }
    }
}

void WindowManager::openGifRecorderWidget(const QRect& captureRect)
{
    if (modules_) {
        if (auto* capture = modules_->module<CaptureModule>()) {
            capture->openGifRecorder(captureRect);
        }
    }
}

bool WindowManager::setScreenshotGlobalKey(const QString& key)
{
    if (modules_) {
        if (auto* shell = modules_->module<ShellModule>()) {
            return shell->setScreenshotGlobalKey(key);
        }
    }
    return false;
}

bool WindowManager::setPinGlobalKey(const QString& key)
{
    if (modules_) {
        if (auto* shell = modules_->module<ShellModule>()) {
            return shell->setPinGlobalKey(key);
        }
    }
    return false;
}

bool WindowManager::setChatGlobalKey(const QString& key)
{
    if (modules_) {
        if (auto* shell = modules_->module<ShellModule>()) {
            return shell->setChatGlobalKey(key);
        }
    }
    return false;
}

bool WindowManager::setLocalSearchGlobalKey(const QString& key)
{
    if (modules_) {
        if (auto* shell = modules_->module<ShellModule>()) {
            return shell->setLocalSearchGlobalKey(key);
        }
    }
    return false;
}

QString WindowManager::lastHotkeyError() const
{
    if (modules_) {
        if (auto* shell = modules_->module<ShellModule>()) {
            return shell->lastHotkeyError();
        }
    }
    return QString();
}

void WindowManager::rebuildLocalSearchIndex()
{
    if (modules_) {
        if (auto* search = modules_->module<LocalSearchModule>()) search->rebuildIndex();
    }
}

QString WindowManager::localSearchStatus() const
{
    if (modules_) {
        if (auto* search = modules_->module<LocalSearchModule>()) return search->statusText();
    }
    return QStringLiteral("未初始化");
}

void WindowManager::showAllSticker()
{
    if (modules_) {
        if (auto* capture = modules_->module<CaptureModule>()) {
            capture->showAllStickers();
        }
    }
}

int WindowManager::allStickerCount()
{
    if (modules_) {
        if (auto* capture = modules_->module<CaptureModule>()) {
            return capture->stickerCount();
        }
    }
    return 0;
}

void WindowManager::showLlmChatWindow(const QString& title, const QString& text, const QPixmap& pixmap)
{
    if (modules_) {
        if (auto* assistant = modules_->module<AssistantModule>()) {
            assistant->showChat(title, text, pixmap);
        }
    }
}

void WindowManager::onTextSelectionActionTriggered(const QString& actionId,
                                                   const QString& selectedText,
                                                   const QString& inputText)
{
    if (!modules_) {
        return;
    }
    auto* assistant = modules_->module<AssistantModule>();
    if (!assistant) {
        return;
    }

    if (actionId == QStringLiteral("chat")) {
        const QString trimmedInput = inputText.trimmed();
        if (!trimmedInput.isEmpty()) {
            assistant->showChat(QStringLiteral("AI 对话"),
                                QStringLiteral("%1\n\n%2").arg(selectedText, trimmedInput));
        } else {
            assistant->showChat(QStringLiteral("AI 对话"));
            assistant->quoteText(selectedText);
        }
        return;
    }

    const QList<TextSelectionActionConfig> actions = settingModel_->textSelectionActions();
    for (const TextSelectionActionConfig& action : actions) {
        if (action.id != actionId) {
            continue;
        }
        const QString label = action.label.trimmed().isEmpty() ? actionId : action.label.trimmed();
        const QString promptText = action.prompt.trimmed();
        const QString prompt = promptText.isEmpty()
            ? selectedText
            : QStringLiteral("%1\n\n%2").arg(selectedText, promptText);
        assistant->showChat(label, prompt);
        return;
    }
}
