#pragma once

#include <QColor>

#include "core/theme/AppTheme.h"

struct ThemeTokens
{
    AppTheme theme = AppTheme::Dark;

    QColor canvas;
    QColor surface;
    QColor surfaceRaised;
    QColor surfaceSubtle;
    QColor border;
    QColor borderStrong;
    QColor textPrimary;
    QColor textSecondary;
    QColor textDisabled;
    QColor accent;
    QColor accentHover;
    QColor accentPressed;
    QColor accentSubtle;
    QColor success;
    QColor danger;
    QColor warning;
    QColor shadow;

    int fontCaption = 12;
    int fontBody = 13;
    int fontSubtitle = 15;
    int fontTitle = 18;

    int spaceXs = 4;
    int spaceSm = 8;
    int spaceMd = 12;
    int spaceLg = 16;
    int spaceXl = 24;

    int radiusSm = 6;
    int radiusMd = 10;
    int radiusLg = 14;

    int compactControlHeight = 32;
    int controlHeight = 34;
    int searchControlHeight = 48;
};

ThemeTokens createThemeTokens(AppTheme theme);
