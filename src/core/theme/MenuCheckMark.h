#pragma once

#include <QAction>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

#include "core/theme/ThemeManager.h"

namespace MenuCheckMark {

inline QIcon icon(bool checked)
{
    if (!checked) {
        // Transparent placeholder keeps the icon column reserved so labels stay aligned.
        static const QIcon kUnchecked = []() {
            QPixmap pm(16, 16);
            pm.fill(Qt::transparent);
            return QIcon(pm);
        }();
        return kUnchecked;
    }

    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(ThemeManager::tokens().textPrimary, 1.8);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    QPainterPath path;
    path.moveTo(3.5, 8.5);
    path.lineTo(6.5, 11.5);
    path.lineTo(12.5, 4.5);
    p.drawPath(path);
    return QIcon(pm);
}

inline void apply(QAction* action, bool checked)
{
    if (action) {
        action->setIcon(icon(checked));
    }
}

}  // namespace MenuCheckMark
