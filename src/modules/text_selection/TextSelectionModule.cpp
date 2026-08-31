#include "modules/text_selection/TextSelectionModule.h"

#include "modules/text_selection/GlobalTextSelectionManager.h"

#include <QDebug>

TextSelectionModule::TextSelectionModule(SettingModel* settings)
    : settings_(settings)
{
    qInfo() << "TextSelectionModule: created";
}

TextSelectionModule::~TextSelectionModule() = default;

void TextSelectionModule::initialize()
{
    qInfo() << "TextSelectionModule::initialize";
    manager_ = std::make_unique<GlobalTextSelectionManager>(this);
    manager_->setSettingModel(settings_);
    connect(manager_.get(), &GlobalTextSelectionManager::sigActionTriggered,
            this, &TextSelectionModule::actionTriggered);
    qInfo() << "TextSelectionModule::initialize: done";
}

void TextSelectionModule::shutdown()
{
    qInfo() << "TextSelectionModule::shutdown";
    manager_.reset();
}
