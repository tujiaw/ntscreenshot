#pragma once

#include <QString>

enum class AppTheme {
    Dark = 0,
    Light = 1
};

inline QString appThemeToSettingValue(AppTheme theme)
{
    if (theme == AppTheme::Light) {
        return QStringLiteral("light");
    }
    return QStringLiteral("dark");
}

inline AppTheme appThemeFromSettingValue(const QString &value)
{
    if (value.compare(QStringLiteral("light"), Qt::CaseInsensitive) == 0) {
        return AppTheme::Light;
    }
    return AppTheme::Dark;
}
