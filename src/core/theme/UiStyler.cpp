#include "core/theme/UiStyler.h"

#include <QStyle>
#include <QWidget>

void UiStyler::setRole(QWidget* widget, UiRole role)
{
    if (!widget) return;
    const char* value = "secondary";
    switch (role) {
    case UiRole::PrimaryButton: value = "primary"; break;
    case UiRole::SecondaryButton: value = "secondary"; break;
    case UiRole::GhostButton: value = "ghost"; break;
    case UiRole::DangerButton: value = "danger"; break;
    case UiRole::IconButton: value = "icon"; break;
    case UiRole::Card: value = "card"; break;
    case UiRole::SearchField: value = "search"; break;
    }
    widget->setProperty("uiRole", QString::fromLatin1(value));
    if (widget->style()) {
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
    }
    widget->update();
}
