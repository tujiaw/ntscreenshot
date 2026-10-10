#include <QCoreApplication>
#include "app/WindowManager.h"

#include "app/ModuleRegistry.h"
#include "app/shell/ShellModule.h"

#include <QDebug>
#include "core/foundation/Constants.h"

#include "modules/capture/CaptureModule.h"
#include "modules/settings/SettingsModule.h"

#include "modules/local_search/LocalSearchModule.h"
#include "modules/image_browser/ImageBrowserModule.h"

WindowManager::WindowManager()
    : settingModel_(std::make_unique<SettingModel>(nullptr))
    , httpServer_(std::make_unique<HttpServerController>(this))
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
}

void WindowManager::destroy()
{
    if (httpServer_) {
        httpServer_->stop();
    }
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
    } else if (id == WidgetID::IMAGE_BROWSER) {
        if (auto* browser = modules_->module<ImageBrowser::ImageBrowserModule>()) {
            browser->open();
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
    return QCoreApplication::translate("App", "未初始化");
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
