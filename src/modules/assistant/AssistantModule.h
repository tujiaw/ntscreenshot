#pragma once

#include "core/modules/IToolModule.h"

#include <QObject>
#include <QPixmap>
#include <QPointer>

class ChatWidget;
class SettingModel;

class AssistantModule final : public QObject, public IToolModule {
    Q_OBJECT

public:
    explicit AssistantModule(SettingModel* settings);

    QString id() const override { return QStringLiteral("assistant"); }
    void initialize() override;
    void shutdown() override;

    void showChat(const QString& title, const QString& text = {}, const QPixmap& pixmap = {});
    void quoteText(const QString& text);

private:
    void showChatImpl(const QString& title, const QString& text, const QPixmap& pixmap);

    SettingModel* settings_ = nullptr;
    QPointer<ChatWidget> chatWindow_;
};
