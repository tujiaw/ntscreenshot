#pragma once

#include <QApplication>
#include <QIcon>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPixmapCache>
#include <QSize>
#include <QString>

#include "core/theme/ThemeManager.h"

enum class IconTone {
    Default,
    Muted,
    Accent,
    Danger,
    OnAccent
};

class ThemeIcon
{
public:
    static QPixmap pixmap(const QString &name, IconTone tone = IconTone::Default,
                          int logicalSize = 0)
    {
        const QString cacheKey = QStringLiteral("nt-theme-icon:%1:%2:%3:%4")
            .arg(name)
            .arg(static_cast<int>(tone))
            .arg(logicalSize)
            .arg(isDarkTheme() ? 1 : 0);
        QPixmap cached;
        if (QPixmapCache::find(cacheKey, &cached)) return cached;

        QPixmap source(QStringLiteral(":/images/") + name);
        if (source.isNull()) {
            return source;
        }
        if (logicalSize > 0) {
            source = source.scaled(logicalSize, logicalSize, Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation);
        }
        const QPixmap result = recolor(source, toneColor(tone));
        QPixmapCache::insert(cacheKey, result);
        return result;
    }

    static QIcon icon(const QString &name, IconTone tone = IconTone::Default,
                      int logicalSize = 0)
    {
        return QIcon(pixmap(name, tone, logicalSize));
    }

private:
    static bool isDarkTheme()
    {
        return ThemeManager::tokens().theme == AppTheme::Dark;
    }

    static QColor toneColor(IconTone tone)
    {
        const ThemeTokens& tokens = ThemeManager::tokens();
        switch (tone) {
        case IconTone::Muted:
            return tokens.textSecondary;
        case IconTone::Accent:
            return tokens.accent;
        case IconTone::Danger:
            return tokens.danger;
        case IconTone::OnAccent:
            return Qt::white;
        case IconTone::Default:
        default:
            return tokens.textPrimary;
        }
    }

    static QPixmap recolor(const QPixmap &source, const QColor &color)
    {
        QPixmap result = source;
        QPainter painter(&result);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(result.rect(), color);
        painter.end();
        return result;
    }
};
