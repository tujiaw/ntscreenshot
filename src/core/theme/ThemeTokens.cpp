#include "core/theme/ThemeTokens.h"

ThemeTokens createThemeTokens(AppTheme theme)
{
    ThemeTokens value;
    value.theme = theme;
    if (theme == AppTheme::Light) {
        value.canvas = QColor(QStringLiteral("#F4F6F8"));
        value.surface = QColor(QStringLiteral("#FFFFFF"));
        value.surfaceRaised = QColor(QStringLiteral("#FFFFFF"));
        value.surfaceSubtle = QColor(QStringLiteral("#F7F8FA"));
        value.border = QColor(QStringLiteral("#E1E5EA"));
        value.borderStrong = QColor(QStringLiteral("#C8D0DA"));
        value.textPrimary = QColor(QStringLiteral("#172033"));
        value.textSecondary = QColor(QStringLiteral("#667085"));
        value.textDisabled = QColor(QStringLiteral("#A5ADBA"));
        value.accent = QColor(QStringLiteral("#3B82F6"));
        value.accentHover = QColor(QStringLiteral("#2563EB"));
        value.accentPressed = QColor(QStringLiteral("#1D4ED8"));
        value.accentSubtle = QColor(QStringLiteral("#EAF2FF"));
        value.success = QColor(QStringLiteral("#16A36A"));
        value.danger = QColor(QStringLiteral("#DC3545"));
        value.warning = QColor(QStringLiteral("#D97706"));
        value.shadow = QColor(15, 23, 42, 30);
    } else {
        value.canvas = QColor(QStringLiteral("#171A20"));
        value.surface = QColor(QStringLiteral("#20242C"));
        value.surfaceRaised = QColor(QStringLiteral("#272C35"));
        value.surfaceSubtle = QColor(QStringLiteral("#2C323D"));
        value.border = QColor(QStringLiteral("#363D49"));
        value.borderStrong = QColor(QStringLiteral("#4A5565"));
        value.textPrimary = QColor(QStringLiteral("#F3F5F8"));
        value.textSecondary = QColor(QStringLiteral("#A7B0BF"));
        value.textDisabled = QColor(QStringLiteral("#687384"));
        value.accent = QColor(QStringLiteral("#5B9BFF"));
        value.accentHover = QColor(QStringLiteral("#78ACFF"));
        value.accentPressed = QColor(QStringLiteral("#3B82F6"));
        value.accentSubtle = QColor(QStringLiteral("#253A59"));
        value.success = QColor(QStringLiteral("#45C58A"));
        value.danger = QColor(QStringLiteral("#FF6B78"));
        value.warning = QColor(QStringLiteral("#F6B44B"));
        value.shadow = QColor(0, 0, 0, 90);
    }
    return value;
}
