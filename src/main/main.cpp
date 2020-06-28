#include <QtWidgets/QApplication>
#include "controller/WindowManager.h"
#include "common/DarkStyle.h"
#include "common/RunGuard.h"
#include "common/Util.h"
#include "common/LogHandler.h"

int main(int argc, char *argv[])
{	
	RunGuard guard("69619FA7-4944-4CCA-BF69-83323F34D32F");
	if (!guard.tryToRun()) {
		return 0;
	}

#ifndef DEBUG
    qInstallMessageHandler(myMessageOutput);
#endif

	QApplication a(argc, argv);
	a.setWindowIcon(QIcon(":/images/ntscreenshot.ico"));
    qDebug() << "=================ntscreenshot start===============";
	CDarkStyle::setFontFamily("Microsoft YaHei", false);
	CDarkStyle::assign();

	a.setQuitOnLastWindowClosed(false);
	QObject::connect(&a, &QApplication::aboutToQuit, []{ WindowManager::instance()->destory(); });
	WindowManager::instance()->openWidget(WidgetID::MAIN);

	return a.exec();
}
