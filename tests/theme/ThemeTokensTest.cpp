#include "core/theme/ThemeTokens.h"

#include <QColor>
#include <QDebug>

#include <cmath>

namespace {

double linearChannel(int channel)
{
    const double value = channel / 255.0;
    return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
}

double luminance(const QColor& color)
{
    return 0.2126 * linearChannel(color.red())
        + 0.7152 * linearChannel(color.green())
        + 0.0722 * linearChannel(color.blue());
}

double contrastRatio(const QColor& first, const QColor& second)
{
    const double high = qMax(luminance(first), luminance(second));
    const double low = qMin(luminance(first), luminance(second));
    return (high + 0.05) / (low + 0.05);
}

bool validateTheme(const ThemeTokens& tokens, const char* label)
{
    const QList<QColor> required{
        tokens.canvas, tokens.surface, tokens.surfaceRaised, tokens.surfaceSubtle,
        tokens.border, tokens.textPrimary, tokens.textSecondary, tokens.accent,
        tokens.accentSubtle, tokens.success, tokens.danger
    };
    for (const QColor& color : required) {
        if (!color.isValid()) {
            qCritical() << label << "contains an invalid color token";
            return false;
        }
    }
    if (contrastRatio(tokens.textPrimary, tokens.surface) < 4.5) {
        qCritical() << label << "primary text contrast is below WCAG AA";
        return false;
    }
    if (tokens.compactControlHeight < 32 || tokens.controlHeight < 34
        || tokens.searchControlHeight < 48) {
        qCritical() << label << "interactive size tokens are too small";
        return false;
    }
    if (!(tokens.spaceXs < tokens.spaceSm && tokens.spaceSm < tokens.spaceMd
          && tokens.spaceMd < tokens.spaceLg && tokens.spaceLg < tokens.spaceXl)) {
        qCritical() << label << "spacing scale is not strictly increasing";
        return false;
    }
    return true;
}

} // namespace

int main()
{
    const ThemeTokens light = createThemeTokens(AppTheme::Light);
    const ThemeTokens dark = createThemeTokens(AppTheme::Dark);
    if (!validateTheme(light, "light") || !validateTheme(dark, "dark")) return 1;
    if (light.surface == dark.surface || light.accent == dark.accent) {
        qCritical() << "light and dark theme tokens must remain distinct";
        return 1;
    }
    qInfo() << "Theme token contract passed";
    return 0;
}
