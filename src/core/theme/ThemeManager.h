#pragma once

#include <QString>

#include "core/theme/AppTheme.h"
#include "core/theme/ThemeTokens.h"

class ThemeManager
{
public:
    static void apply(AppTheme theme);
    static const ThemeTokens& tokens();
    static QString webCssVariables();

private:
    static QString loadStyleSheet();
    static QString buildStyleSheet(const ThemeTokens& tokens);
};
