#include "app/MyApplication.h"
#include "app/ModuleRegistry.h"
#include "app/WindowManager.h"
#include "app/shell/ShellModule.h"
#include "modules/assistant/AssistantModule.h"
#include "modules/capture/CaptureModule.h"
#include "modules/clipboard/ClipboardLiteManager.h"
#include "modules/settings/SettingsModule.h"
#include "modules/text_selection/TextSelectionModule.h"
#include "modules/local_search/LocalSearchModule.h"
#include "core/theme/DarkStyle.h"
#include "core/runtime/RunGuard.h"
#include "core/platform/Util.h"
#include "core/runtime/LogHandler.h"
#include "app/crash/MiniDump.h"

#include <QIcon>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

int main(int argc, char *argv[])
{
    qInfo() << "main: entry, pid =" << QCoreApplication::applicationPid();
    MiniDump::EnableAutoDump();
    qInstallMessageHandler(myMessageOutput);
    qInfo() << "main: log handler installed";

#ifdef Q_OS_WIN
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif

    // 禁用 Qt 内部的高DPI缩放，确保所有的 QRect 坐标与 Windows 原生物理像素 1:1 对应
    qputenv("QT_ENABLE_HIGHDPI_SCALING", "0");
    qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "0");

    // 单实例：避免重复启动（托盘/全局快捷键类应用尤其需要）
    qInfo() << "main: checking single instance guard...";
    RunGuard guard("ntscreenshot_run_guard");
    if (!guard.tryToRun()) {
        qInfo() << "main: another instance is already running, exiting";
        return 0;
    }
    qInfo() << "main: single instance guard acquired";

    qInfo() << "main: constructing QApplication...";
	MyApplication a(argc, argv);
	a.setWindowIcon(QIcon(":/images/ntscreenshot.ico"));
    qInfo() << "main: QApplication constructed, appName =" << a.applicationName();
    qDebug() << "=================ntscreenshot start===============";
#ifdef Q_OS_WIN
    CDarkStyle::setFontFamily(QStringLiteral("Microsoft YaHei"), false);
#else
    CDarkStyle::setFontFamily(QString(), false);
#endif
    qInfo() << "main: creating WindowManager...";
	WindowManager windowManager;
    qInfo() << "main: WindowManager created, registering modules...";
	ModuleRegistry modules;
	auto* clipboard = modules.emplaceModule<ClipboardLiteManager>();
    qInfo() << "main: registered ClipboardLiteManager";
	modules.emplaceModule<CaptureModule>(&windowManager);
    qInfo() << "main: registered CaptureModule";
	modules.emplaceModule<AssistantModule>(windowManager.setting());
    qInfo() << "main: registered AssistantModule";
	modules.emplaceModule<SettingsModule>(&windowManager);
    qInfo() << "main: registered SettingsModule";
	modules.emplaceModule<TextSelectionModule>(windowManager.setting());
    qInfo() << "main: registered TextSelectionModule";
	auto* localSearch = modules.emplaceModule<LocalSearchModule>(windowManager.setting());
    qInfo() << "main: registered LocalSearchModule";
	modules.emplaceModule<ShellModule>(&windowManager, clipboard);
    qInfo() << "main: registered ShellModule, total modules =" << modules.moduleIds().size();
	windowManager.setModuleRegistry(&modules);
	QObject::connect(&windowManager, &WindowManager::sigSettingChanged,
	                 localSearch, &LocalSearchModule::applySettings);
	CDarkStyle::assign(windowManager.setting()->themeMode());


	QApplication::setQuitOnLastWindowClosed(false);
	QObject::connect(&a, &QApplication::aboutToQuit, &windowManager, [&windowManager] {
        qInfo() << "main: aboutToQuit signal, destroying window manager...";
        windowManager.destroy();
        qInfo() << "main: window manager destroyed";
    });
    qInfo() << "main: initializing all modules...";
	modules.initializeAll();
    qInfo() << "main: all modules initialized, opening main widget...";
	windowManager.openWidget(WidgetID::MAIN);
    if (qEnvironmentVariableIsSet("NTSCREENSHOT_QA_SETTINGS")) {
        QTimer::singleShot(10000, &a, [&windowManager] {
            windowManager.openWidget(WidgetID::SETTINGS);
        });
    }
    qInfo() << "main: entering event loop...";

	int result = a.exec();
    qInfo() << "main: event loop exited, result =" << result;
	return result;
}
