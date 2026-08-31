#pragma once

#include <QString>
#include <QDir>
#include <QSize>
#include <QColor>

struct GitHubImageBedConfig {
    QString owner;
    QString repo;
    QString branch;
    QString token;
    QString pathPrefix;
    QString cdnUrl;
};

namespace WidgetID {
	const QString MAIN = "MAIN";
	const QString SETTINGS = "SETTINGS";
	const QString SCREENSHOT = "SCREENSHOT";
	const QString TEXT_SELECTION = "TEXT_SELECTION";
    const QString LOCAL_SEARCH = "LOCAL_SEARCH";
    const QString LONG_SCREENSHOT = "LONG_SCREENSHOT";
    const QString GIF_RECORDER = "GIF_RECORDER";
    const QString MASK = "MASK";
}

namespace Style {
    const QColor AUTO_SELECTED_BORDER_COLOR = QColor("#1AAD19"); //QColor(0, 175, 255);
	const int AUTO_SELECTED_BORDER_WIDTH = 4;
    const QColor  DRAW_SELECTED_BORDER_COLOR = QColor("#1AAD19"); //QColor(0, 174, 255)
    const int DRAW_SELECTED_BORDER_WIDTH = 1;
    const QSize OERECT_FIXED_SIZE(95, 25);
    const QColor OERECT_BACKGROUND(0, 0, 0, 0);
    const QString FRAMELESS_BROKDER_ACTIVE = "#1883D7";
    const QString FRAMELESS_BROKDER_DEACTIVE = "#AAAAAA";
}
