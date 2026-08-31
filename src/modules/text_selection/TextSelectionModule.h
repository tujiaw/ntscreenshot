#pragma once

#include "core/modules/IToolModule.h"

#include <QObject>

#include <memory>

class GlobalTextSelectionManager;
class SettingModel;

class TextSelectionModule final : public QObject, public IToolModule {
    Q_OBJECT

public:
    explicit TextSelectionModule(SettingModel* settings);
    ~TextSelectionModule() override;

    QString id() const override { return QStringLiteral("text_selection"); }
    void initialize() override;
    void shutdown() override;

signals:
    void actionTriggered(const QString& actionId, const QString& selectedText, const QString& inputText);

private:
    SettingModel* settings_ = nullptr;
    std::unique_ptr<GlobalTextSelectionManager> manager_;
};
