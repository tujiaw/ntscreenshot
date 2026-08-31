#include "modules/assistant/AssistantModule.h"

#include "core/platform/Util.h"
#include "core/settings/SettingModel.h"
#include "modules/assistant/ui/ChatWidget.h"

#include <QApplication>
#include <QDebug>
#include <QScreen>
#include <QSettings>
#include <QTimer>

namespace {
void saveGeometry(const QWidget* widget)
{
    if (!widget) {
        return;
    }
    QSettings settings(Util::getConfigPath(), QSettings::IniFormat);
    const QString group = QStringLiteral("modules/assistant/ui/window/");
    settings.setValue(group + QStringLiteral("width"), widget->width());
    settings.setValue(group + QStringLiteral("height"), widget->height());
    settings.setValue(group + QStringLiteral("x"), widget->x());
    settings.setValue(group + QStringLiteral("y"), widget->y());
}

bool restoreGeometry(QWidget* widget)
{
    if (!widget) {
        return false;
    }
    QSettings settings(Util::getConfigPath(), QSettings::IniFormat);
    const QString group = QStringLiteral("modules/assistant/ui/window/");
    if (!settings.contains(group + QStringLiteral("width"))) {
        return false;
    }
    const QSize minimum = widget->minimumSize();
    const QSize size(qMax(settings.value(group + QStringLiteral("width")).toInt(), minimum.width()),
                     qMax(settings.value(group + QStringLiteral("height")).toInt(), minimum.height()));
    const QPoint position(settings.value(group + QStringLiteral("x")).toInt(),
                          settings.value(group + QStringLiteral("y")).toInt());
    widget->setGeometry(Util::fixWindowGeometry(QRect(position, size), minimum));
    return true;
}

void positionWindow(QWidget* widget, const SettingModel* settings)
{
    if (!widget || !settings || !QApplication::primaryScreen()) {
        return;
    }
    const auto position = settings->trayNotificationPosition();
    const bool top = position == TrayNotificationPosition::TopLeft ||
                     position == TrayNotificationPosition::TopRight;
    const bool left = position == TrayNotificationPosition::TopLeft ||
                      position == TrayNotificationPosition::BottomLeft;
    const QRect available = QApplication::primaryScreen()->availableGeometry();
    const int margin = Util::scaleSize(20);
    widget->move(left ? available.left() + margin : available.right() - widget->width() - margin + 1,
                 top ? available.top() + margin : available.bottom() - widget->height() - margin + 1);
}
}

AssistantModule::AssistantModule(SettingModel* settings)
    : settings_(settings)
{
    qInfo() << "AssistantModule: created";
}

void AssistantModule::initialize()
{
    qInfo() << "AssistantModule::initialize";
}

void AssistantModule::shutdown()
{
    qInfo() << "AssistantModule::shutdown";
    if (chatWindow_) {
        saveGeometry(chatWindow_);
        chatWindow_->close();
        chatWindow_.clear();
    }
}

void AssistantModule::showChat(const QString& title, const QString& text, const QPixmap& pixmap)
{
    QTimer::singleShot(0, this, [this, title, text, pixmap]() {
        showChatImpl(title, text, pixmap);
    });
}

void AssistantModule::quoteText(const QString& text)
{
    QTimer::singleShot(0, this, [this, text]() {
        if (chatWindow_) {
            chatWindow_->quoteTextInInput(text);
        }
    });
}

void AssistantModule::showChatImpl(const QString& title, const QString& text, const QPixmap& pixmap)
{
    if (!chatWindow_) {
        const QSize size = settings_ ? settings_->notificationWindowSize() : QSize();
        chatWindow_ = new ChatWidget(settings_, title, QString(), size);
        connect(chatWindow_, &ChatWidget::closed, this, [this](ChatWidget* widget) {
            if (chatWindow_ == widget) {
                saveGeometry(chatWindow_);
                chatWindow_.clear();
            }
        });
        if (!restoreGeometry(chatWindow_)) {
            positionWindow(chatWindow_, settings_);
        }
    }
    chatWindow_->show();
    chatWindow_->raise();
    chatWindow_->activateWindow();

    if (!pixmap.isNull()) {
        if (!text.trimmed().isEmpty()) {
            chatWindow_->quoteTextInInput(text.trimmed());
        }
        chatWindow_->quoteImageInInput(pixmap, QStringLiteral("image.png"));
    } else if (!text.trimmed().isEmpty()) {
        chatWindow_->startChatWithText(text);
    }
}
