#include "component/Application.h"
#include "controller/WindowManager.h"
#include "common/DarkStyle.h"
#include "common/RunGuard.h"
#include "common/Util.h"
#include "common/LogHandler.h"

int main(int argc, char *argv[])
{
    qInstallMessageHandler(myMessageOutput);
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	Application a(argc, argv);
	a.setWindowIcon(QIcon(":/images/ntscreenshot.ico"));
    qDebug() << "=================ntscreenshot start===============";
	CDarkStyle::setFontFamily("Microsoft YaHei", false);
	CDarkStyle::assign();

	QApplication::setQuitOnLastWindowClosed(false);
	QObject::connect(&a, &QApplication::aboutToQuit, []{ WindowManager::instance()->destory(); });
	WindowManager::instance()->openWidget(WidgetID::MAIN);

	return a.exec();
}
