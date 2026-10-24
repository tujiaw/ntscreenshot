#pragma once

#include "core/platform/Util.h"
#include "core/theme/ThemeManager.h"
#include <QHBoxLayout>
#include <QPainter>
#include <QWidget>

namespace CaptureControlPanelStyle {

inline constexpr int ButtonSize = 30;
inline constexpr int IconSize = 16;

inline void setup(QWidget* panel, QHBoxLayout* layout)
{
    layout->setContentsMargins(Util::scaleSize(8), Util::scaleSize(6),
                              Util::scaleSize(8), Util::scaleSize(6));
    layout->setSpacing(Util::scaleSize(6));
    panel->setFixedHeight(Util::scaleSize(42));
}

inline void paint(QWidget* panel)
{
    QPainter painter(panel);
    painter.setRenderHint(QPainter::Antialiasing);
    const ThemeTokens& tokens = ThemeManager::tokens();
    painter.setPen(QPen(tokens.borderStrong, 1));
    painter.setBrush(tokens.surfaceSubtle);
    painter.drawRoundedRect(QRectF(panel->rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                            Util::scaleSize(8), Util::scaleSize(8));
}

} // namespace CaptureControlPanelStyle
