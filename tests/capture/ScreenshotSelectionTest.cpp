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
#include <QProcess>
#include <QKeyEvent>
#include <QClipboard>
#include <QMimeData>
#include <QEventLoop>
#include <QPushButton>
#include "core/platform/Util.h"

int runScreenshotSelectionTest(QApplication& app) {
    app.setApplicationName(QStringLiteral("ntscreenshot-selection-test"));
    QSettings clean(Util::getConfigPath(), QSettings::IniFormat);
    clean.setValue(QStringLiteral("AUTO_PIN"), false);
    clean.setValue(QStringLiteral("OPENCV_MODE"), false);
    clean.setValue(QStringLiteral("AUTO_SAVE"), false);
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
        bool okay=screenshot && screenshot->isVisible() && panel && panel->isVisible() && closeCount==0;
#ifdef Q_OS_WIN
        okay &= screenshot && GetPropW(reinterpret_cast<HWND>(screenshot->winId()), L"AutoBrowser.IgnoreTextSelection");
        const QString probePath = qEnvironmentVariable("NT_TEXT_SELECTION_POLICY_PROBE");
        if (!probePath.isEmpty() && screenshot) {
            QProcess probe;
            probe.start(probePath, {QStringLiteral("--window"), QString::number(screenshot->winId())});
            okay &= probe.waitForFinished(10000) && probe.exitStatus() == QProcess::NormalExit && probe.exitCode() == 0;
        }
#endif
        // Let queued close/selection callbacks run before editing the selection.
        QEventLoop settle;
        QTimer::singleShot(500, &settle, &QEventLoop::quit);
        settle.exec();
        okay &= screenshot && screenshot->isVisible() && closeCount == 0;
        if (!okay) { manager.destroy(); app.exit(2); return; }
        const QPoint toolbarBeforeResize = panel->pos();
        mouse(QEvent::MouseMove,{799,375},Qt::NoButton,Qt::NoButton);
        mouse(QEvent::MouseButtonPress,{799,375},Qt::LeftButton,Qt::LeftButton);
        mouse(QEvent::MouseMove,{900,375},Qt::NoButton,Qt::LeftButton);
        mouse(QEvent::MouseButtonRelease,{900,375},Qt::LeftButton,Qt::NoButton);
        okay &= panel->pos() != toolbarBeforeResize && screenshot->isVisible() && closeCount == 0;
        qInfo() << "Resize:" << toolbarBeforeResize << panel->pos() << okay;
        DrawMode line(DrawMode::Line);
        line.pen().setColor(Qt::red);
        line.pen().setWidth(3);
        for (auto* button : panel->findChildren<QPushButton*>())
            if (button->toolTip() == QStringLiteral("直线")) button->click();
        mouse(QEvent::MouseMove,{400,300},Qt::NoButton,Qt::NoButton);
        panel->drawer()->setMode(line);
        qInfo() << "Before drawing:" << panel->drawer()->enable() << panel->drawer()->isDraw();
        mouse(QEvent::MouseButtonPress,{400,300},Qt::LeftButton,Qt::LeftButton);
        mouse(QEvent::MouseMove,{500,300},Qt::NoButton,Qt::LeftButton);
        mouse(QEvent::MouseButtonRelease,{500,300},Qt::LeftButton,Qt::NoButton);
        QPixmap annotation(1000,700);
        annotation.fill(Qt::white);
        panel->drawer()->drawPixmap(annotation);
        okay &= annotation.toImage().pixelColor(450,300) == QColor(Qt::red) && closeCount == 0;
        qInfo() << "Drawing:" << annotation.toImage().pixelColor(450,300) << okay;
        // Verify explicit completion still works and restore the user's clipboard.
        auto* previous = new QMimeData;
        if (const auto* data = QApplication::clipboard()->mimeData())
            for (const auto& format : data->formats()) previous->setData(format, data->data(format));
        QKeyEvent finish(QEvent::KeyPress, Qt::Key_C, Qt::ControlModifier);
        QApplication::sendEvent(screenshot, &finish);
        const auto capture = QApplication::clipboard()->pixmap();
        okay &= closeCount == 1 && !capture.isNull() && capture.width() > 601;
        qInfo() << "Completion:" << closeCount << capture.size() << okay;
        QApplication::clipboard()->setMimeData(previous);
        QApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
        okay &= screenshot.isNull();
        qInfo() << "Selection, delayed callbacks, resize, drawing and explicit completion:" << okay;
        manager.destroy();
        app.exit(okay ? 0 : 2);
    });
    return app.exec();
}
