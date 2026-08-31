#include "app/ModuleRegistry.h"

#include "core/modules/IToolModule.h"

#include <QDebug>

ModuleRegistry::~ModuleRegistry()
{
    shutdownAll();
}

bool ModuleRegistry::registerModule(std::unique_ptr<IToolModule> module)
{
    if (!module) {
        qWarning() << "Cannot register a null module";
        return false;
    }
    if (initialized_) {
        qWarning() << "Cannot register a module after initialization:" << module->id();
        return false;
    }
    for (const auto& registered : modules_) {
        if (registered->id() == module->id()) {
            qWarning() << "Duplicate tool module id:" << module->id();
            return false;
        }
    }

    modules_.push_back(std::move(module));
    return true;
}

IToolModule* ModuleRegistry::module(const QString& id) const
{
    for (const auto& entry : modules_) {
        if (entry->id() == id) {
            return entry.get();
        }
    }
    return nullptr;
}

QStringList ModuleRegistry::moduleIds() const
{
    QStringList ids;
    ids.reserve(static_cast<qsizetype>(modules_.size()));
    for (const auto& entry : modules_) {
        ids.append(entry->id());
    }
    return ids;
}

void ModuleRegistry::initializeAll()
{
    if (initialized_) {
        qInfo() << "ModuleRegistry: already initialized, skipping";
        return;
    }
    qInfo() << "ModuleRegistry: initializing" << modules_.size() << "modules...";
    for (const auto& module : modules_) {
        qInfo() << "ModuleRegistry: initializing module" << module->id() << "...";
        module->initialize();
        ++initializedCount_;
        qInfo() << "ModuleRegistry: module" << module->id() << "initialized (" << initializedCount_ << "/" << modules_.size() << ")";
    }
    initialized_ = true;
    qInfo() << "ModuleRegistry: all modules initialized";
}

void ModuleRegistry::shutdownAll()
{
    qInfo() << "ModuleRegistry: shutting down" << initializedCount_ << "modules...";
    while (initializedCount_ > 0) {
        qInfo() << "ModuleRegistry: shutting down module" << modules_[initializedCount_ - 1]->id() << "...";
        modules_[initializedCount_ - 1]->shutdown();
        --initializedCount_;
    }
    initialized_ = false;
    qInfo() << "ModuleRegistry: all modules shut down";
}
