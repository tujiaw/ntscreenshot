/*
###############################################################################
#                                                                             #
# The MIT License                                                             #
#                                                                             #
# Copyright (C) 2017 by Juergen Skrotzky (JorgenVikingGod@gmail.com)          #
#               >> https://github.com/Jorgen-VikingGod                        #
#                                                                             #
# Sources: https://github.com/Jorgen-VikingGod/Qt-Frameless-Window-DarkStyle  #
#                                                                             #
###############################################################################
*/

#ifndef _CDarkStyle_HPP
#define _CDarkStyle_HPP

#include <QtCore>
#include <QtGui>
#include <QStyleFactory>

class CDarkStyle
{
public:
	static void assign()
	{
		// set style
		qApp->setStyle(QStyleFactory::create("Fusion"));
		
		// modify palette to dark
		QPalette darkPalette;
		darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
		darkPalette.setColor(QPalette::WindowText, Qt::white);
		darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(127, 127, 127));
		darkPalette.setColor(QPalette::Base, QColor(42, 42, 42));
		darkPalette.setColor(QPalette::AlternateBase, QColor(66, 66, 66));
		darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
		darkPalette.setColor(QPalette::ToolTipText, Qt::white);
		darkPalette.setColor(QPalette::Text, Qt::white);
		darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
		darkPalette.setColor(QPalette::Dark, QColor(35, 35, 35));
		darkPalette.setColor(QPalette::Shadow, QColor(20, 20, 20));
		darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
		darkPalette.setColor(QPalette::ButtonText, Qt::white);
		darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));
		darkPalette.setColor(QPalette::BrightText, Qt::red);
		darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
		darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
		darkPalette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(80, 80, 80));
		darkPalette.setColor(QPalette::HighlightedText, Qt::white);
		darkPalette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(127, 127, 127));

		qApp->setPalette(darkPalette);
		// loadstylesheet
		QFile qfDarkstyle(QString(":/darkstyle/darkstyle.qss"));
		if (qfDarkstyle.open(QIODevice::ReadOnly | QIODevice::Text))
		{
			// set stylesheet
			QString qsStylesheet = QString(qfDarkstyle.readAll());
			qApp->setStyleSheet(qsStylesheet);
			qfDarkstyle.close();
		}
	}

	static void setFontFamily(const QString &fontFamily, bool isBold)
	{
		QFont defaultFont = QApplication::font();
		static int fontSize = defaultFont.pointSize();
		if (!fontFamily.isEmpty()) {
			defaultFont.setFamily(fontFamily);
		}
		defaultFont.setBold(isBold);
		defaultFont.setPointSize(fontSize);
		qApp->setFont(defaultFont);
	}
};

#endif  // _CDarkStyle_HPP

