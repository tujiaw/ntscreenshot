#pragma once

class QWidget;

enum class UiRole {
    PrimaryButton,
    SecondaryButton,
    GhostButton,
    DangerButton,
    IconButton,
    Card,
    SearchField
};

class UiStyler
{
public:
    static void setRole(QWidget* widget, UiRole role);
};
