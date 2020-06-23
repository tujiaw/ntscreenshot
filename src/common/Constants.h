#ifndef CONSTANTS_H_
#define CONSTANTS_H_

#include <QString>
#include <QDir>
#include <QSize>
#include <QColor>

namespace WidgetID {
	const QString MAIN = "MAIN";
	const QString SETTINGS = "SETTINGS";
	const QString SCREENSHOT = "SCREENSHOT";
}

namespace Style {
	const QColor SELECTED_BORDER_COLOR = QColor(0, 175, 255);
	const int SELECTED_BORDER_WIDTH = 4;
	const QColor STICKER_BORDER_COLOR = QColor(24, 131, 215, 200);
	const int STICKER_BORDER_WIDTH = 2;
    const QSize OERECT_FIXED_SIZE(95, 25);
    const QColor OERECT_BACKGROUND(0, 0, 0, 0);
    const QSize OEAMPLIFIER_SIZE(150, 150);
    const int OEAMPLIFIER_IMAGE_HEIGHT = 110;
}

#endif // CONSTANTS_H_
