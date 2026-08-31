#include "modules/settings/SettingsModule.h"

#include "modules/settings/Settings.h"

#include <QDebug>

SettingsModule::SettingsModule(WindowManager* windowManager)
    : windowManager_(windowManager)
{
    qInfo() << "SettingsModule: created";
}

void SettingsModule::initialize()
{
    qInfo() << "SettingsModule::initialize";
}

void SettingsModule::shutdown()
{
    qInfo() << "SettingsModule::shutdown";
    close();
}

void SettingsModule::open()
{
    qInfo() << "SettingsModule::open: start";
    if (!window_) {
        qInfo() << "SettingsModule::open: creating settings dialog...";
        auto* settings = new Settings(windowManager_);
        window_ = settings;
        qInfo() << "SettingsModule::open: settings dialog created";
    }
    qInfo() << "SettingsModule::open: showing window...";
    window_->show();
    window_->raise();
    window_->activateWindow();
    qInfo() << "SettingsModule::open: window shown";
}

void SettingsModule::close()
{
    if (window_) {
        QWidget* window = window_;
        window_.clear();
        // Settings 设置 WA_DeleteOnClose，close() 后自动删除。
        window->close();
    }
}
