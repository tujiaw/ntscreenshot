#include "app/shell/ShellModule.h"

#include "app/shell/MainWidget.h"
#include "modules/clipboard/ClipboardLiteManager.h"
#include "shared/ui/FramelessWidget.h"

#include <QDebug>

ShellModule::ShellModule(WindowManager* windowManager, ClipboardLiteManager* clipboard)
    : windowManager_(windowManager)
    , clipboard_(clipboard)
{
    qInfo() << "ShellModule: created";
}

void ShellModule::initialize()
{
    qInfo() << "ShellModule::initialize: creating MainWidget...";
    auto* frame = new FramelessWidget();
    auto* mainWidget = new MainWidget(windowManager_, clipboard_, frame);
    frame->setContent(mainWidget);
    frame->hide();
    frame_ = frame;
    mainWidget_ = mainWidget;
    qInfo() << "ShellModule::initialize: done";
}

void ShellModule::shutdown()
{
    qInfo() << "ShellModule::shutdown";
    if (frame_) {
        QWidget* frame = frame_;
        frame_.clear();
        mainWidget_.clear();
        frame->close();
        frame->deleteLater();
    }
}

void ShellModule::open()
{
    if (!frame_) {
        initialize();
    }
}

bool ShellModule::setScreenshotGlobalKey(const QString& key)
{
    return mainWidget_ && mainWidget_->setScreenshotGlobalKey(key);
}

bool ShellModule::setPinGlobalKey(const QString& key)
{
    return mainWidget_ && mainWidget_->setPinGlobalKey(key);
}

bool ShellModule::setChatGlobalKey(const QString& key)
{
    return mainWidget_ && mainWidget_->setChatGlobalKey(key);
}

bool ShellModule::setLocalSearchGlobalKey(const QString& key)
{
    return mainWidget_ && mainWidget_->setLocalSearchGlobalKey(key);
}

QString ShellModule::lastHotkeyError() const
{
    return mainWidget_ ? mainWidget_->lastHotkeyError() : QString();
}
