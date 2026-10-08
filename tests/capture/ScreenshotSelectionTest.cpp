#include "app/WindowManager.h"
#include "app/ModuleRegistry.h"
#include "modules/capture/CaptureModule.h"
#include "modules/capture/screenshot/Screenshot.h"
#include "modules/capture/annotation/DrawPanel.h"
#include "core/theme/ThemeManager.h"
#include <QApplication>
#include <QMouseEvent>
#include <QPointer>
#include <QTimer>
#include <QDebug>
#include <QSettings>
#include "core/platform/Util.h"

int runScreenshotSelectionTest(QApplication& app) {
    app.setApplicationName(QStringLiteral("ntscreenshot-selection-test"));
    QSettings clean(Util::getConfigPath(), QSettings::IniFormat);
    clean.setValue(QStringLiteral("AUTO_PIN"), false);
    clean.setValue(QStringLiteral("OPENCV_MODE"), false);
    WindowManager manager;
    ModuleRegistry modules;
    modules.emplaceModule<CaptureModule>(&manager);
    manager.setModuleRegistry(&modules);
    modules.initializeAll();
    ThemeManager::apply(AppTheme::Dark);
    QTimer::singleShot(0, &app, [&] {
        manager.openWidget(WidgetID::SCREENSHOT);
        QPointer<ScreenshotWidget> screenshot;
        for(auto* w: QApplication::topLevelWidgets()) if(auto* s=qobject_cast<ScreenshotWidget*>(w)) screenshot=s;
        if(!screenshot) { app.exit(1); return; }
        int closeCount=0;
        QObject::connect(screenshot, &ScreenshotWidget::sigClose, &app, [&] { ++closeCount; });
        auto mouse=[&](QEvent::Type type, QPoint p, Qt::MouseButton button, Qt::MouseButtons held) {
            QMouseEvent event(type, QPointF(p), QPointF(screenshot->mapToGlobal(p)), button, held, Qt::NoModifier);
            QApplication::sendEvent(screenshot, &event);
        };
        mouse(QEvent::MouseButtonPress,{200,200},Qt::LeftButton,Qt::LeftButton);
        mouse(QEvent::MouseMove,{800,550},Qt::NoButton,Qt::LeftButton);
        mouse(QEvent::MouseButtonRelease,{800,550},Qt::LeftButton,Qt::NoButton);
        QApplication::processEvents();
        auto* panel=screenshot ? screenshot->findChild<DrawPanel*>() : nullptr;
        qInfo() << "Selection release:" << "close count" << closeCount << "toolbar visible" << (panel && panel->isVisible());
        const bool okay=screenshot && screenshot->isVisible() && panel && panel->isVisible() && closeCount==0;
        manager.destroy();
        app.exit(okay ? 0 : 2);
    });
    return app.exec();
}
