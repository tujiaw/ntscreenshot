#include "app/WindowManager.h"
#include "app/ModuleRegistry.h"
#include "modules/capture/CaptureModule.h"
#include "modules/capture/screenshot/Screenshot.h"
#include "modules/capture/annotation/DrawPanel.h"
#include "core/theme/ThemeManager.h"
#include "core/settings/SettingModel.h"
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
#include <QTextEdit>
#include <QScrollBar>
#include <QEnterEvent>
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
        const auto previousCursorStyle = manager.setting()->screenshotCursorStyle();
        const QColor previousBorderColor = manager.setting()->selectionBorderColor();
        const QColor testBorderColor(QStringLiteral("#AA44EE"));
        bool cursorOptionsOkay = true;
        for (auto style : {ScreenshotCursorStyle::Color, ScreenshotCursorStyle::Classic,
                           ScreenshotCursorStyle::Contrast, ScreenshotCursorStyle::System}) {
            manager.setting()->setScreenshotCursorStyle(style);
            SettingModel reloaded(nullptr);
            cursorOptionsOkay &= reloaded.screenshotCursorStyle() == style;
            const QCursor cursor = Util::screenshotCursor(style);
            cursorOptionsOkay &= style == ScreenshotCursorStyle::System
                ? cursor.shape() == Qt::CrossCursor : !cursor.pixmap().isNull();
            if (style == ScreenshotCursorStyle::Classic) {
                const qreal scale = qMax(1.0, Util::getScreenScaleFactor());
                cursorOptionsOkay &= cursor.pixmap().size() == QSize(qRound(29 * scale), qRound(45 * scale));
                cursorOptionsOkay &= cursor.hotSpot() == QPoint(qRound(15 * scale), qRound(23 * scale));
            }
        }
        manager.setting()->setScreenshotCursorStyle(ScreenshotCursorStyle::Classic);
        manager.setting()->setSelectionBorderColor(testBorderColor);
        {
            SettingModel reloaded(nullptr);
            cursorOptionsOkay &= reloaded.selectionBorderColor() == testBorderColor;
            reloaded.setSelectionBorderColor(QColor());
            cursorOptionsOkay &= reloaded.selectionBorderColor() == testBorderColor;
        }
        manager.openWidget(WidgetID::SCREENSHOT);
        QPointer<ScreenshotWidget> screenshot;
        for(auto* w: QApplication::topLevelWidgets()) if(auto* s=qobject_cast<ScreenshotWidget*>(w)) screenshot=s;
        manager.setting()->setScreenshotCursorStyle(previousCursorStyle);
        manager.setting()->setSelectionBorderColor(previousBorderColor);
        if(!screenshot) { app.exit(1); return; }
        cursorOptionsOkay &= screenshot->cursor().hotSpot() == Util::screenshotCursor(ScreenshotCursorStyle::Classic).hotSpot();
        qInfo() << "Cursor styles, saved preference and classic hotspot:" << cursorOptionsOkay;
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
        bool okay=cursorOptionsOkay && screenshot && screenshot->isVisible() && panel && panel->isVisible() && closeCount==0;
        const QColor paintedBorder = screenshot->grab(QRect(430, 200, 1, 1)).toImage().pixelColor(0, 0);
        okay &= paintedBorder == testBorderColor;
        qInfo() << "Saved selection border color painted correctly:" << paintedBorder << testBorderColor;
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
        // The visual gap is transparent toolbar padding: it receives hover
        // without forwarding it to the selection's cursor/resize handler.
        const bool toolbarBelow = panel->y() > 549;
        okay &= toolbarBelow ? panel->geometry().top() == 550
                             : panel->geometry().bottom() == 199;
        const QPoint bridgePoint(panel->width() / 2, toolbarBelow ? 0 : panel->height() - 1);
        okay &= screenshot->childAt(panel->mapTo(screenshot, bridgePoint)) == panel;
        okay &= panel->cursor().shape() == Qt::ArrowCursor;
        const QCursor previousCursor = screenshot->cursor();
        screenshot->setCursor(Qt::ForbiddenCursor);
        QMouseEvent bridgeHover(QEvent::MouseMove, QPointF(bridgePoint),
            QPointF(panel->mapToGlobal(bridgePoint)), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(panel, &bridgeHover);
        okay &= bridgeHover.isAccepted() && screenshot->cursor().shape() == Qt::ForbiddenCursor;
        screenshot->setCursor(previousCursor);
        const QPoint selectionEdge(panel->geometry().center().x(), toolbarBelow ? 549 : 200);
        QWidget* edgeTarget = screenshot->childAt(selectionEdge);
        okay &= edgeTarget != panel && (!edgeTarget || !panel->isAncestorOf(edgeTarget));
        const QRect bottomAnchor(200, Util::desktopLocalRect().height() - 50, 600, 40);
        panel->onReferRectChanged(bottomAnchor);
        okay &= panel->geometry().bottom() == bottomAnchor.top() - 1;
        const QPoint upperBridge(panel->width() / 2, panel->height() - 1);
        okay &= screenshot->childAt(panel->mapTo(screenshot, upperBridge)) == panel;
        screenshot->moveDrawPanel();
        qInfo() << "Continuous toolbar hover and accessible selection edge:" << okay;
        // Expiring a result must restore the exact toolbar/settings height,
        // even after Qt processes the queued layout and native resize events.
        auto verifyResultExpiry = [&](DrawPanel* target) {
          const QPoint previousMousePosition = QCursor::pos();
          QCursor::setPos(target->mapToGlobal(QPoint(0, -4)));
          for (bool settingsOpen : {false, true}) {
            if (settingsOpen) target->onColorBtnClicked();
            QApplication::processEvents();
            const int originalHeight = target->height();
            for (int attempt = 0; attempt < 2; ++attempt) {
                const QString resultText = attempt == 0 ? QStringLiteral("OCR result\nFirst line\nSecond line")
                    : QStringLiteral("Long OCR result, preserved in full for scrolling and copying.\n").repeated(80);
                target->showToolMessage(resultText);
                QApplication::processEvents();
                okay &= target->height() > originalHeight;
                auto* expiry = target->findChild<QTimer*>(QString(), Qt::FindDirectChildrenOnly);
                auto* result = target->findChild<QTextEdit*>(QStringLiteral("DrawPanelToolMessage"));
                okay &= result && result->height() <= Util::scaleSize(160) && result->toPlainText() == resultText;
                if (attempt == 1 && result && expiry) {
                    okay &= result->verticalScrollBar()->maximum() > 0;
                    result->verticalScrollBar()->setValue(result->verticalScrollBar()->maximum());
                    for (QWidget* hoverTarget : {static_cast<QWidget*>(result), result->viewport(),
                                                 static_cast<QWidget*>(result->verticalScrollBar())}) {
                        QEnterEvent enter(QPointF(1, 1), QPointF(1, 1),
                                          QPointF(hoverTarget->mapToGlobal(QPoint(1, 1))));
                        QApplication::sendEvent(hoverTarget, &enter);
                        okay &= !expiry->isActive();
                        QMetaObject::invokeMethod(expiry, "timeout", Qt::DirectConnection);
                        okay &= result->isVisible();
                        QEvent leave(QEvent::Leave);
                        QApplication::sendEvent(hoverTarget, &leave);
                        QApplication::processEvents();
                        okay &= expiry->isActive();
                    }
                }
                okay &= expiry && QMetaObject::invokeMethod(expiry, "timeout", Qt::DirectConnection);
                QEventLoop layoutSettle;
                QTimer::singleShot(50, &layoutSettle, &QEventLoop::quit);
                layoutSettle.exec();
                qInfo() << "Expired result height:" << target->isWindow() << settingsOpen << originalHeight << target->height();
                okay &= target->height() == originalHeight;
            }
            if (settingsOpen) target->onColorBtnClicked();
          }
          QCursor::setPos(previousMousePosition);
        };
        verifyResultExpiry(panel);
        {
            DrawPanel floating(nullptr, nullptr, screenshot);
            floating.onReferRectChanged(QRect(screenshot->mapToGlobal(QPoint(200, 200)), QSize(600, 350)));
            floating.show();
            QApplication::processEvents();
            verifyResultExpiry(&floating);
        }
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
