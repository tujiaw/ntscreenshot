#pragma once

#include "core/modules/IToolModule.h"

#include <QPointer>

class QWidget;
class WindowManager;

class SettingsModule final : public IToolModule {
public:
    explicit SettingsModule(WindowManager* windowManager);

    QString id() const override { return QStringLiteral("settings"); }
    void initialize() override;
    void shutdown() override;

    void open();
    void close();

private:
    WindowManager* windowManager_ = nullptr;
    QPointer<QWidget> window_;
};
