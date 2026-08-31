#pragma once

#include <QColor>

#include "core/theme/ThemeManager.h"

namespace OverlayTheme {

inline bool isDarkTheme()
{
    return ThemeManager::tokens().theme == AppTheme::Dark;
}

inline QColor notificationBackgroundColor()
{
    if (isDarkTheme()) {
        return QColor(40, 44, 52, 230);
    }
    return QColor(250, 250, 252, 245);
}

inline QColor notificationBorderColor()
{
    if (isDarkTheme()) {
        return QColor(60, 64, 72, 255);
    }
    return QColor(210, 214, 220, 255);
}

}  // namespace OverlayTheme
