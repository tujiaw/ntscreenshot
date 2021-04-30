#pragma once

#include <QString>
#include <QDir>
#include <QSize>
#include <QColor>

namespace WidgetID {
	const QString MAIN = "MAIN";
	const QString SETTINGS = "SETTINGS";
	const QString SCREENSHOT = "SCREENSHOT";
    const QString MASK = "MASK";
}

namespace Style {
    const QColor AUTO_SELECTED_BORDER_COLOR = QColor("#1AAD19"); //QColor(0, 175, 255);
	const int AUTO_SELECTED_BORDER_WIDTH = 4;
    const QColor  DRAW_SELECTED_BORDER_COLOR = QColor("#1AAD19"); //QColor(0, 174, 255)
    const int DRAW_SELECTED_BORDER_WIDTH = 1;
    const QString FRAMELESS_BROKDER_ACTIVE = "#1883D7";
    const QString FRAMELESS_BROKDER_DEACTIVE = "#AAAAAA";
    const QSize OERECT_FIXED_SIZE(95, 25);
    const QColor OERECT_BACKGROUND(0, 0, 0, 0);
}
