#pragma once

#include <QtCore>
#include <QtGui>
#include <QApplication>
#include <QFile>
#include <QStyle>
#include <QStyleFactory>
#include "core/theme/AppTheme.h"
#include "core/theme/ThemeManager.h"
#include "core/platform/Util.h"

class CDarkStyle
{
public:
	static void assign(AppTheme theme = AppTheme::Dark)
	{
		ThemeManager::apply(theme);
	}

	static void setFontFamily(const QString &fontFamily, bool isBold)
	{
		QFont defaultFont = QApplication::font();
		if (!fontFamily.isEmpty()) {
			defaultFont.setFamily(fontFamily);
		}
		defaultFont.setBold(isBold);
		// 使用像素字号，避免 Per-Monitor V2 DPI 模式下系统字体引擎按屏幕DPI放大 point 字号
		defaultFont.setPixelSize(Util::scaleSize(12));
		qApp->setFont(defaultFont);
	}

};


