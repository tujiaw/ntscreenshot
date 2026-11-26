#include "modules/capture/long_screenshot/LongScreenshotControlPanel.h"
#include "app/WindowManager.h"
#include "app/ModuleRegistry.h"
#include "modules/capture/CaptureModule.h"
#include "modules/capture/screenshot/Screenshot.h"
#include "modules/capture/screenshot/ScreenshotActionController.h"
#include "modules/capture/annotation/DrawPanel.h"
#include "core/theme/ThemeManager.h"
#include "core/settings/SettingModel.h"
#include "modules/capture/pin/PinWidget.h"
#include "modules/capture/annotation/OcrSelectionOverlay.h"
#include <QTcpServer>
#include <QElapsedTimer>
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
#include <QScreen>
#include <QTextEdit>
#include <QScrollBar>
#include <QEnterEvent>
#include <QPainter>
#include <QWheelEvent>
#include <QTemporaryDir>
#include <QFile>
#include "shared/ui/TipsWidget.h"
#include "core/platform/Util.h"
#include "shared/ui/FramelessWidget.h"

static int runScreenshotAutoSaveTest(QApplication& app) {
    app.setApplicationName(QStringLiteral("ntscreenshot-auto-save-test"));
    app.setQuitOnLastWindowClosed(false);
    QTemporaryDir temporary;
    if (!temporary.isValid()) return 1;
    WindowManager manager;
    CaptureModule capture(&manager);
    const QString directory = temporary.path() + QStringLiteral("/截图/nested");
    QPixmap image(32, 24);
    image.fill(Qt::magenta);
    QPainter painter(&image);
    painter.fillRect(5, 6, 9, 8, Qt::green);
    painter.end();
    bool okay = true;
    auto check = [&](bool condition, const char* message) {
        if (!condition) qWarning() << "Auto-save test failed:" << message;
        okay &= condition;
    };
    auto flush = [] {
        QApplication::processEvents();
        QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    };
    auto complete = [&](bool sticker = false, bool cancel = false, bool nullImage = false) {
        capture.openScreenshot();
        ScreenshotActionController* controller = nullptr;
        for (QWidget* widget : QApplication::topLevelWidgets()) {
            if (auto* screenshot = qobject_cast<ScreenshotWidget*>(widget))
                controller = screenshot->findChild<ScreenshotActionController*>();
        }
        check(controller != nullptr, "action controller exists");
        if (!controller) return;
        if (cancel) controller->sigClose();
        else if (sticker) controller->onStickerRequested(image, QPoint(10, 10));
        else controller->onSaveToClipboardRequested(nullImage ? QPixmap() : image);
        flush();
    };
    auto images = [&] { return QDir(directory).entryList({QStringLiteral("*.png")}, QDir::Files); };
    manager.setting()->setAutoSaveImage(false, directory);
    complete();
    check(!QDir(directory).exists(), "disabled does not create a directory");
    manager.setting()->setAutoSaveImage(true, directory);
    complete();
    check(images().size() == 1, "completion creates missing directories and a PNG");
    check(QApplication::clipboard()->pixmap().toImage() == image.toImage(), "clipboard preserved");
    complete();
    complete(true);
    check(images().size() == 3, "rapid captures and sticker create separate files");
    for (const QString& name : images()) {
        check(QImage(QDir(directory).filePath(name)).convertToFormat(QImage::Format_ARGB32)
            == image.toImage().convertToFormat(QImage::Format_ARGB32), "saved pixels preserve annotations");
    }
    complete(false, true);
    complete(false, false, true);
    capture.closeScreenshot();
    flush();
    check(images().size() == 3, "cancel and empty image do not save");
    const QString blocked = temporary.path() + QStringLiteral("/blocked");
    QFile file(blocked);
    check(file.open(QIODevice::WriteOnly), "create blocked directory fixture");
    file.write("unchanged");
    file.close();
    manager.setting()->setAutoSaveImage(true, blocked + QStringLiteral("/child"));
    complete();
    bool failureVisible = false;
    for (QWidget* widget : QApplication::topLevelWidgets()) {
        if (auto* tip = qobject_cast<TipsWidget*>(widget)) {
            failureVisible |= tip->isVisible() && tip->text().contains(blocked);
            tip->close();
        }
    }
    check(failureVisible, "failure is visible to the user");
    check(QApplication::clipboard()->pixmap().toImage() == image.toImage(), "failure preserves clipboard");
    check(file.open(QIODevice::ReadOnly) && file.readAll() == "unchanged", "failure does not damage existing files");
    manager.setting()->setAutoSaveImage(false, directory);
    for (PinWidget* pin : PinWidget::getAllSticker()) pin->parentWidget()->close();
    capture.shutdown();
    flush();
    qInfo() << "Screenshot auto-save workflow:" << okay;
    return okay ? 0 : 2;
}

int runStampAnnotationTest(QApplication& app) {
    QWidget canvas;
    canvas.resize(1000, 700);
    DrawPanel panel(nullptr, &canvas, &canvas);
    const QRect bounds(20, 20, 950, 500);
    panel.onReferRectChanged(bounds);
    panel.drawer()->setEnable(true);
    canvas.show(); panel.show(); app.processEvents();
    auto* stamp = panel.findChild<QPushButton*>(QStringLiteral("DrawPanelStampButton"));
    auto* picker = panel.findChild<QWidget*>(QStringLiteral("DrawPanelStampPicker"));
    bool okay = true;
    auto check = [&](bool condition, const char* message) {
        if (!condition) qWarning() << "Stamp test failed:" << message;
        okay &= condition;
    };
    check(stamp && picker && picker->isWindow(), "picker is a separate floating window");
    if (!stamp || !picker) return 1;
    const QSize toolbarSize = panel.size();
    stamp->click(); app.processEvents();
    check(picker->isVisible() && panel.size() == toolbarSize, "opening picker does not expand toolbar");
    auto choose = [&](int index) {
        auto* button = picker->findChild<QPushButton*>(QStringLiteral("stampChoice%1").arg(index));
        check(button != nullptr, "stamp choice exists");
        if (button) button->click();
    };
    choose(0);
    check(!picker->isVisible(), "choosing a stamp closes picker");
    auto* more = panel.findChild<QPushButton*>(QStringLiteral("DrawPanelMoreButton"));
    auto* morePopup = panel.findChild<QWidget*>(QStringLiteral("DrawPanelMoreTools"));
    check(more && morePopup && morePopup->isWindow(), "more tools use a floating window");
    if (more && morePopup) {
        stamp->click(); more->click(); app.processEvents();
        check(morePopup->isVisible() && !picker->isVisible() && panel.size() == toolbarSize,
              "more tools replace stamp picker without resizing toolbar");
        stamp->click(); app.processEvents();
        check(picker->isVisible() && !morePopup->isVisible() && !more->isChecked(),
              "stamp picker replaces more tools");
        choose(0);
    }
    auto* shapes = panel.findChild<QPushButton*>(QStringLiteral("DrawPanelShapesButton"));
    auto* shapePopup = panel.findChild<QWidget*>(QStringLiteral("DrawPanelShapePopup"));
    check(shapes && shapePopup && shapePopup->isWindow(), "shapes share one toolbar button and floating picker");
    if (shapes && shapePopup) {
        shapes->click(); app.processEvents();
        check(shapePopup->isVisible() && !picker->isVisible() && !morePopup->isVisible()
                  && panel.size() == toolbarSize, "shape picker replaces other popups without resizing toolbar");
        check(shapes->isChecked() && panel.drawer()->mode().shape() == DrawMode::Arrow,
              "first use of the shape button activates the default arrow");
        const auto buttons = shapePopup->findChildren<QPushButton*>();
        const QList<DrawMode::Shape> modes = {DrawMode::Arrow, DrawMode::Rectangle, DrawMode::PolyLine,
                                             DrawMode::Line, DrawMode::Ellipse, DrawMode::Highlighter};
        check(buttons.size() == modes.size(), "shape picker contains all six tools");
        const auto genericShapeIcon = shapes->icon().cacheKey();
        for (int i = 0; i < qMin(buttons.size(), modes.size()); ++i) {
            if (!shapePopup->isVisible()) shapes->click();
            buttons.at(i)->click();
            check(panel.drawer()->mode().shape() == modes.at(i) && shapes->isChecked()
                      && shapes->icon().cacheKey() == genericShapeIcon
                      && !shapePopup->isVisible(), "choosing a shape activates it and preserves generic toolbar icon");
        }
        panel.cancelChecked();
        check(!shapes->isChecked() && panel.drawer()->mode().isNone(), "cancel clears shape selection");
        shapes->click(); app.processEvents();
        check(shapes->isChecked() && panel.drawer()->mode().shape() == DrawMode::Highlighter,
              "shape button restores the last shape after cancellation without choosing again");
        choose(0);
        shapes->click(); app.processEvents();
        check(shapes->isChecked() && !stamp->isChecked()
                  && panel.drawer()->mode().shape() == DrawMode::Highlighter && shapePopup->isVisible(),
              "returning from another tool restores the last shape and offers the picker");
        shapes->click(); app.processEvents();
        check(!shapePopup->isVisible() && panel.drawer()->mode().shape() == DrawMode::Highlighter,
              "closing the shape picker keeps the remembered shape active");
        choose(0);
    }
    auto click = [&](QPoint pos, Qt::MouseButton button = Qt::LeftButton) {
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(pos), QPointF(canvas.mapToGlobal(pos)),
                          button, button, Qt::NoModifier);
        app.sendEvent(&canvas, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(pos), QPointF(canvas.mapToGlobal(pos)),
                            button, Qt::NoButton, Qt::NoModifier);
        app.sendEvent(&canvas, &release);
    };
    auto output = [&] {
        QPixmap pixmap(canvas.size()); pixmap.fill(Qt::transparent);
        panel.drawer()->drawPixmap(pixmap);
        return pixmap.toImage();
    };
    QImage empty = output(); empty.fill(Qt::transparent);
    check(output() == empty, "no unplaced stamp is exported");
    click(QPoint(160, 140)); click(QPoint(260, 140));
    check(panel.drawer()->nextStampNumber() == 3, "number stamps increment from one");
    const int size = Util::scaleSize(32);
    const QImage expected = Stamp::image(Stamp::Number, QColor("#DC3545"), 2, size);
    check(output().copy(QRect(QPoint(260 - size / 2, 140 - size / 2), QSize(size, size)))
              .convertToFormat(expected.format()) == expected, "second exported stamp contains number two");
    panel.drawer()->undo();
    check(panel.drawer()->nextStampNumber() == 2 && output().pixelColor(260, 140).alpha() == 0,
          "undo removes stamp and restores sequence");
    choose(1); click(QPoint(260, 140));
    check(panel.drawer()->nextStampNumber() == 3, "switching colors preserves sequence");
    for (int i = 4; i < 8; ++i) {
        choose(i); click(QPoint(360 + (i - 4) * 100, 140));
    }
    check(panel.drawer()->nextStampNumber() == 3, "symbols do not consume numbers");
    choose(0);
    const QImage beforeInvalidClicks = output();
    click(QPoint(5, 5)); click(QPoint(160, 240), Qt::RightButton);
    check(panel.drawer()->nextStampNumber() == 3 && output() == beforeInvalidClicks,
          "outside and right clicks do not place stamps or consume numbers");
    panel.drawer()->clearHistory();
    check(panel.drawer()->nextStampNumber() == 1, "new canvas starts numbering at one");
    auto* highlighter = panel.findChild<QPushButton*>(QStringLiteral("DrawPanelHighlighterButton"));
    check(highlighter != nullptr, "highlighter is available in shape picker");
    if (highlighter) {
        highlighter->click();
        DrawMode mode = panel.getMode();
        check(mode.shape() == DrawMode::Highlighter && mode.isPathShape()
                  && mode.pen().width() > DrawSettings::penWidth()
                  && mode.pen().color() == DrawSettings::currentColor(),
              "highlighter uses a wide stroke and the selected color");
        mode.pen().setColor(Qt::yellow); mode.pen().setWidth(20);
        panel.drawer()->setMode(mode);
        auto strokeEvent = [&](QEvent::Type type, QPoint pos, Qt::MouseButton button, Qt::MouseButtons held) {
            QMouseEvent event(type, QPointF(pos), QPointF(canvas.mapToGlobal(pos)), button, held, Qt::NoModifier);
            app.sendEvent(&canvas, &event);
        };
        strokeEvent(QEvent::MouseButtonPress, {100, 300}, Qt::LeftButton, Qt::LeftButton);
        for (QPoint pos : {QPoint(250, 300), QPoint(150, 300), QPoint(200, 300)})
            strokeEvent(QEvent::MouseMove, pos, Qt::NoButton, Qt::LeftButton);
        strokeEvent(QEvent::MouseButtonRelease, {300, 300}, Qt::LeftButton, Qt::NoButton);
        const QImage highlighted = output();
        check(highlighted.pixelColor(160, 300).alpha() == 96
                  && highlighted.pixelColor(280, 300).alpha() == 96,
              "retracing within one highlighter stroke keeps uniform transparency");
        QPixmap textBackground(canvas.size()); textBackground.fill(Qt::white);
        { QPainter painter(&textBackground); painter.fillRect(QRect(190, 294, 10, 12), Qt::black); }
        panel.drawer()->drawPixmap(textBackground);
        check(textBackground.toImage().pixelColor(195, 300) == QColor(Qt::black)
                  && textBackground.toImage().pixelColor(160, 300).blue() < 255,
              "highlighter tints the background while preserving dark text");
        panel.drawer()->undo();
        check(output() == empty, "undo removes the entire highlighter stroke");
        click(QPoint(160, 300));
        check(output().pixelColor(160, 300).alpha() == 96, "a click places a highlighter dot");
        panel.drawer()->undo();
    }
    DrawMode ellipse(DrawMode::Ellipse);
    ellipse.pen().setWidth(2);
    panel.drawer()->setMode(ellipse);
    auto ellipseEvent = [&](QEvent::Type type, QPoint pos, Qt::KeyboardModifiers modifiers) {
        QMouseEvent event(type, QPointF(pos), QPointF(canvas.mapToGlobal(pos)),
                          type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
                          type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton, modifiers);
        app.sendEvent(&canvas, &event);
    };
    auto inkBounds = [&](const QImage& image) {
        QRect bounds;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                if (image.pixelColor(x, y).alpha()) bounds |= QRect(x, y, 1, 1);
        return bounds;
    };
    const QPoint origin(450, 250);
    for (QPoint delta : {QPoint(70, 30), QPoint(-70, 30), QPoint(70, -30), QPoint(-70, -30),
                         QPoint(0, 70), QPoint(70, 0)}) {
        ellipseEvent(QEvent::MouseButtonPress, origin, Qt::ShiftModifier);
        ellipseEvent(QEvent::MouseMove, origin + delta, Qt::ShiftModifier);
        const QImage preview = output();
        const QRect bounds = inkBounds(preview);
        check(bounds.width() > 60 && bounds.width() == bounds.height(), "Shift constrains ellipse to circle in every direction");
        ellipseEvent(QEvent::MouseButtonRelease, origin + delta, Qt::ShiftModifier);
        check(output() == preview, "committed circle matches live preview");
        panel.drawer()->undo();
        check(output() == empty, "circle can be undone");
    }
    ellipseEvent(QEvent::MouseButtonPress, origin, Qt::NoModifier);
    ellipseEvent(QEvent::MouseMove, origin + QPoint(70, 30), Qt::NoModifier);
    const QImage freeEllipse = output();
    const QRect freeBounds = inkBounds(freeEllipse);
    check(freeBounds.width() > freeBounds.height(), "ellipse remains unconstrained without Shift");
    QKeyEvent shiftDown(QEvent::KeyPress, Qt::Key_Shift, Qt::ShiftModifier);
    app.sendEvent(&canvas, &shiftDown);
    const QRect circleBounds = inkBounds(output());
    check(circleBounds.width() == circleBounds.height(), "pressing Shift updates circle without moving mouse");
    QKeyEvent shiftUp(QEvent::KeyRelease, Qt::Key_Shift, Qt::NoModifier);
    app.sendEvent(&canvas, &shiftUp);
    check(output() == freeEllipse, "releasing Shift restores ellipse without moving mouse");
    ellipseEvent(QEvent::MouseButtonRelease, origin + QPoint(70, 30), Qt::NoModifier);
    check(output() == freeEllipse, "unconstrained ellipse stays unchanged after release");
    panel.drawer()->undo();
    stamp->click(); panel.hide(); app.processEvents();
    check(!picker->isVisible(), "hiding toolbar closes floating picker");
    return okay ? 0 : 1;
}

static int runLongScreenshotPanelTest(QApplication& app) {
    app.setQuitOnLastWindowClosed(false);
    for (auto theme : {AppTheme::Light, AppTheme::Dark}) {
        ThemeManager::apply(theme);
        LongScreenshotControlPanel panel;
        panel.show();
        panel.adjustSize();
        app.processEvents();
        const QSize normalSize = panel.size();
        if (panel.width() > Util::scaleSize(100)) {
            qCritical() << "Normal long screenshot toolbar must contain only two adjacent buttons" << panel.size();
            return 1;
        }
        const QString output = qEnvironmentVariable("NTSCREENSHOT_LONG_PANEL_OUTPUT");
        if (!output.isEmpty()) panel.grab().save(output + (theme == AppTheme::Light ? "/panel-light.png" : "/panel-dark.png"));
        panel.setRecoveryWarning(true);
        app.processEvents();
        if (panel.height() != normalSize.height() || panel.width() >= Util::scaleSize(300)) return 1;
        if (!output.isEmpty()) panel.grab().save(output + (theme == AppTheme::Light ? "/warning-light.png" : "/warning-dark.png"));
        const QSize warningSize = panel.size();
        panel.setRecoveryWarning(true);
        if (panel.size() != warningSize) return 1;
        panel.setRecoveryWarning(false);
        app.processEvents();
        if (panel.size() != normalSize) return 1;
    }
    return 0;
}

int runScreenshotKeyboardTest(QApplication& app);

int runScreenshotSelectionTest(QApplication& app) {
    if (qEnvironmentVariableIsSet("NTSCREENSHOT_KEYBOARD_TEST"))
        return runScreenshotKeyboardTest(app);
    if (qEnvironmentVariableIsSet("NTSCREENSHOT_LONG_PANEL_TEST"))
        return runLongScreenshotPanelTest(app);
    if (qEnvironmentVariableIsSet("NTSCREENSHOT_AUTO_SAVE_TEST"))
        return runScreenshotAutoSaveTest(app);
    app.setApplicationName(QStringLiteral("ntscreenshot-selection-test"));
    if (runStampAnnotationTest(app) != 0) return 1;
    app.setQuitOnLastWindowClosed(false);
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
        const auto previousBorderStyle = manager.setting()->selectionBorderStyle();
        const QColor previousCrossColor = manager.setting()->amplifierCrossColor();
        const QColor testBorderColor(QStringLiteral("#AA44EE"));
        bool cursorOptionsOkay = true;
        manager.setting()->setAmplifierCrossColor(QColor(QStringLiteral("#FF6633")));
        {
            SettingModel reloaded(nullptr);
            const QColor expectedCrossColor(255, 102, 51, 180);
            cursorOptionsOkay &= reloaded.amplifierCrossColor() == expectedCrossColor;
            reloaded.setAmplifierCrossColor(QColor());
            cursorOptionsOkay &= reloaded.amplifierCrossColor() == expectedCrossColor;
        }
        manager.setting()->setAmplifierCrossColor(previousCrossColor);
        for (auto style : {Qt::SolidLine, Qt::DashLine, Qt::DotLine,
                           Qt::DashDotLine, Qt::DashDotDotLine}) {
            manager.setting()->setSelectionBorderStyle(style);
            SettingModel reloaded(nullptr);
            cursorOptionsOkay &= reloaded.selectionBorderStyle() == style;
        }
        for (auto invalid : {Qt::NoPen, Qt::CustomDashLine}) {
            manager.setting()->setSelectionBorderStyle(invalid);
            SettingModel reloaded(nullptr);
            cursorOptionsOkay &= reloaded.selectionBorderStyle() == Qt::SolidLine;
        }
        manager.setting()->setSelectionBorderStyle(previousBorderStyle);
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
        qInfo() << "Continuous toolbar hover and accessible selection edge:" << okay;
        auto* moreButton = panel->findChild<QPushButton*>(QStringLiteral("DrawPanelMoreButton"));
        auto* moreTools = panel->findChild<QWidget*>(QStringLiteral("DrawPanelMoreTools"));
        bool moreToolsOkay = moreButton && moreTools && moreTools->isHidden();
        if (moreToolsOkay) {
            const int collapsedHeight = panel->height();
            const int collapsedWidth = panel->width();
            const QRect nearBottom(200, screenshot->height() - collapsedHeight - 50, 600, 40);
            panel->onReferRectChanged(nearBottom);
            moreToolsOkay &= panel->y() == nearBottom.bottom() + 1;
            moreButton->click();
            QApplication::processEvents();
            moreToolsOkay &= moreTools->isVisible() && moreTools->isWindow() && panel->height() == collapsedHeight;
            moreToolsOkay &= panel->width() == collapsedWidth;
            moreToolsOkay &= panel->y() == nearBottom.bottom() + 1;
            moreToolsOkay &= screenshot->screen()->availableGeometry().contains(moreTools->geometry());
            const auto buttons = moreTools->findChildren<QPushButton*>();
            moreToolsOkay &= buttons.size() == 6;
            for (int i = 0; i < buttons.size(); ++i) {
                auto* button = buttons.at(i);
                moreToolsOkay &= !button->icon().isNull() && !button->text().isEmpty()
                    && button->y() == buttons.at((i / 2) * 2)->y()
                    && button->width() > button->iconSize().width()
                    && button->height() >= button->iconSize().height()
                    && moreTools->rect().contains(button->geometry());
                if (i % 2 == 1) {
                    const int gap = button->x() - buttons.at(i - 1)->geometry().right() - 1;
                    moreToolsOkay &= gap == Util::scaleSize(4);
                } else if (i >= 2) {
                    moreToolsOkay &= button->x() == buttons.first()->x()
                        && button->y() > buttons.at(i - 2)->geometry().bottom();
                }
            }
            if (!buttons.isEmpty()) {
                const QString label = buttons.first()->text();
                const int buttonHeight = buttons.first()->height();
                buttons.first()->setText(QStringLiteral("A label that needs several lines ").repeated(4));
                moreButton->click(); moreButton->click();
                QApplication::processEvents();
                moreToolsOkay &= panel->width() == collapsedWidth && buttons.first()->height() > buttonHeight;
                moreToolsOkay &= panel->y() == nearBottom.bottom() + 1;
                buttons.first()->setText(label);
            }
            moreButton->click();
            QApplication::processEvents();
            moreToolsOkay &= moreTools->isHidden() && panel->height() == collapsedHeight;
            moreToolsOkay &= panel->width() == collapsedWidth;
            moreToolsOkay &= panel->y() == nearBottom.bottom() + 1;
        }
        qInfo() << "More tools float without resizing or moving toolbar:" << moreToolsOkay;
        okay &= moreToolsOkay;
        screenshot->moveDrawPanel();
        // Expiring a result must restore the exact toolbar/settings height,
        // even after Qt processes the queued layout and native resize events.
        auto verifyResultExpiry = [&](DrawPanel* target) {
          const QPoint previousMousePosition = QCursor::pos();
          // A taller result can move the toolbar, so park the cursor away
          // from both possible positions before testing hover and expiry.
          QCursor::setPos(screenshot->mapToGlobal(QPoint(0, 0)));
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
            auto* toggle = floating.findChild<QPushButton*>(QStringLiteral("DrawPanelMoreButton"));
            auto* tools = floating.findChild<QWidget*>(QStringLiteral("DrawPanelMoreTools"));
            bool floatingToolsOkay = toggle && tools;
            if (floatingToolsOkay) {
                const int collapsedHeight = floating.height();
                bool scanRequested = false;
                QObject::connect(&floating, &DrawPanel::sigScanCode, &floating, [&] {
                    scanRequested = true;
                });
                toggle->click();
                const auto buttons = tools->findChildren<QPushButton*>();
                floatingToolsOkay &= tools->isVisible() && buttons.size() == 4;
                for (auto* button : buttons) {
                    if (button->text() == QCoreApplication::translate("App", "扫码")) {
                        const QPoint textPoint(button->width() - Util::scaleSize(8), button->height() / 2);
                        const QPoint globalPoint = button->mapToGlobal(textPoint);
                        QMouseEvent press(QEvent::MouseButtonPress, textPoint, globalPoint,
                                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                        QApplication::sendEvent(button, &press);
                        QMouseEvent release(QEvent::MouseButtonRelease, textPoint, globalPoint,
                                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                        QApplication::sendEvent(button, &release);
                        break;
                    }
                }
                floatingToolsOkay &= scanRequested && tools->isHidden()
                    && !toggle->isChecked() && floating.height() == collapsedHeight;
            }
            qInfo() << "Floating toolbar dispatches and collapses more tools:" << floatingToolsOkay;
            okay &= floatingToolsOkay;
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
        // CaptureModule::closeScreenshot() hides immediately and defers widget
        // destruction with deleteLater() so teardown does not block the copy.
        QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        okay &= screenshot.isNull();
        qInfo() << "Selection, delayed callbacks, resize, drawing and explicit completion:" << okay;
        // Entering OCR from a screenshot must hand off to a pin before starting
        // recognition. Keep the mock server silent to inspect the scanning state.
        QTcpServer ocrServer;
        const auto previousOcr = manager.setting()->paddleOcrConfig();
        okay &= ocrServer.listen(QHostAddress::LocalHost);
        manager.setting()->setPaddleOcrConfig({true,
            QStringLiteral("http://127.0.0.1:%1/jobs").arg(ocrServer.serverPort()),
            QStringLiteral("test"), QStringLiteral("PP-OCRv6")});
        manager.openWidget(WidgetID::SCREENSHOT);
        for (auto* widget : QApplication::topLevelWidgets())
            if (auto* candidate = qobject_cast<ScreenshotWidget*>(widget)) screenshot = candidate;
        if (screenshot) {
            mouse(QEvent::MouseButtonPress, {200,200}, Qt::LeftButton, Qt::LeftButton);
            mouse(QEvent::MouseMove, {800,550}, Qt::NoButton, Qt::LeftButton);
            mouse(QEvent::MouseButtonRelease, {800,550}, Qt::LeftButton, Qt::NoButton);
            auto* ocrPanel = screenshot->findChild<DrawPanel*>();
            if (ocrPanel) {
                ocrPanel->drawer()->setEnable(true);
                ocrPanel->drawer()->setMode(line);
                mouse(QEvent::MouseButtonPress, {300,300}, Qt::LeftButton, Qt::LeftButton);
                mouse(QEvent::MouseMove, {450,300}, Qt::NoButton, Qt::LeftButton);
                mouse(QEvent::MouseButtonRelease, {450,300}, Qt::LeftButton, Qt::NoButton);
                ocrPanel->sigOcr();
            } else okay = false;
        } else okay = false;
        QPointer<PinWidget> pin;
        QPointer<OcrSelectionOverlay> ocrOverlay;
        QElapsedTimer pinWait;
        pinWait.start();
        while (!ocrOverlay && pinWait.elapsed() < 3000) {
            QApplication::processEvents();
            const auto pins = PinWidget::getAllSticker();
            if (!pins.isEmpty()) {
                pin = pins.last();
                ocrOverlay = pin->findChild<OcrSelectionOverlay*>();
            }
        }
        qInfo() << "OCR handoff:" << screenshot.isNull() << bool(pin) << bool(ocrOverlay)
                << (ocrOverlay && ocrOverlay->isRecognizing());
        okay &= screenshot.isNull() && pin && ocrOverlay && ocrOverlay->isRecognizing();
        if (pin && ocrOverlay) {
            okay &= pin->findChild<DrawPanel*>() == nullptr;
            okay &= pin->size() == pin->getPixmap().deviceIndependentSize().toSize();
            const QImage bitmap = pin->getPixmap().toImage();
            qInfo() << "OCR annotation:" << bitmap.size() << bitmap.pixelColor(150,100);
            okay &= bitmap.pixelColor(150,100) == QColor(Qt::red);
            const QImage scanFrame = ocrOverlay->grab().toImage();
            QEventLoop scan;
            QTimer::singleShot(120, &scan, &QEventLoop::quit);
            scan.exec();
            okay &= ocrOverlay->isRecognizing() && scanFrame != ocrOverlay->grab().toImage();
            okay &= pin->getPixmap().toImage() == bitmap;
            QKeyEvent cancel(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            QApplication::sendEvent(ocrOverlay, &cancel);
            okay &= !ocrOverlay->isVisible() && !ocrOverlay->isRecognizing() && pin->isVisible();
            auto disabledOcr = manager.setting()->paddleOcrConfig();
            disabledOcr.enabled = false;
            manager.setting()->setPaddleOcrConfig(disabledOcr);
            QMetaObject::invokeMethod(pin, "onOcr", Qt::DirectConnection);
            QTextEdit* ocrError = nullptr;
            for (auto* widget : QApplication::topLevelWidgets()) {
                if (auto* toolbar = qobject_cast<DrawPanel*>(widget)) {
                    auto* message = toolbar->findChild<QTextEdit*>(QStringLiteral("DrawPanelToolMessage"));
                    if (message && message->isVisible()) ocrError = message;
                }
            }
            okay &= ocrError && ocrError->isVisible()
                && ocrError->toPlainText().contains(QStringLiteral("OCR 功能未启用"));
            okay &= !ocrOverlay->isVisible() && !ocrOverlay->isRecognizing();
            disabledOcr.enabled = true;
            manager.setting()->setPaddleOcrConfig(disabledOcr);
            pin->parentWidget()->close();
        }
        QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        const bool previousNoBorder = manager.setting()->pinNoBorder();
        const QColor previousPinColor = manager.setting()->selectionBorderColor();
        manager.setting()->setSelectionBorderColor(testBorderColor);
        for (bool border : {false, true}) {
            for (qreal dpr : {1.0, 2.0}) {
                manager.setting()->setPinNoBorder(!border);
                QPixmap original(300, 100);
                original.fill(Qt::white);
                {
                    QPainter imagePainter(&original);
                    imagePainter.fillRect(QRect(4, 25, 10, 50), Qt::black);
                    imagePainter.fillRect(QRect(285, 25, 10, 50), Qt::black);
                }
                original.setDevicePixelRatio(dpr);
                PinWidget::popup(&manager, original, QPoint(100, 100));
                QApplication::processEvents();
                const auto pins = PinWidget::getAllSticker();
                if (pins.size() != 1) { okay = false; break; }
                auto* alignedPin = pins.first();
                okay &= alignedPin->size() == original.deviceIndependentSize().toSize();
                auto* frame = qobject_cast<FramelessWidget*>(alignedPin->parentWidget());
                if (frame && border) {
                    auto paintedColor = [frame]() { return frame->grab(QRect(0, 10, 1, 1)).toImage().pixelColor(0, 0); };
                    frame->setActiveStyle();
                    okay &= paintedColor() == testBorderColor;
                    frame->setDeactiveStyle();
                    okay &= paintedColor() == testBorderColor;
                    const QColor updatedColor(QStringLiteral("#22B877"));
                    manager.setting()->setSelectionBorderColor(updatedColor);
                    okay &= paintedColor() == updatedColor;
                    frame->setActiveStyle();
                    okay &= paintedColor() == updatedColor;
                    manager.setting()->setSelectionBorderColor(testBorderColor);
                } else if (frame) {
                    manager.setting()->setSelectionBorderColor(QColor(QStringLiteral("#22B877")));
                    okay &= frame->contentsMargins().isNull();
                    manager.setting()->setSelectionBorderColor(testBorderColor);
                } else okay = false;
                QMetaObject::invokeMethod(alignedPin, "onOcr", Qt::DirectConnection);
                auto* alignedOverlay = alignedPin->findChild<OcrSelectionOverlay*>();
                if (alignedOverlay) {
                    const OcrResult alignment = OcrResult::parse(R"({"rec_texts":["left right"],
                        "text_word":[["left","right"]],
                        "text_word_boxes":[[[4,25,14,75],[285,25,295,75]]]})");
                    alignedOverlay->setResult(alignment, QSize(300, 100));
                    for (int delta : {0, 120, -120}) {
                        if (delta) {
                            QWheelEvent zoom(QPointF(20,20), QPointF(alignedPin->mapToGlobal(QPoint(20,20))),
                                             QPoint(), QPoint(0, delta), Qt::NoButton, Qt::NoModifier,
                                             Qt::NoScrollPhase, false);
                            QApplication::sendEvent(alignedPin, &zoom);
                            QApplication::processEvents();
                        }
                        const QRectF display = alignedPin->imageDisplayRect();
                        const QPointF left(display.left() + display.width() * 9 / 300,
                                           display.top() + display.height() / 2);
                        const QPointF right(display.left() + display.width() * 290 / 300, left.y());
                        QMouseEvent begin(QEvent::MouseButtonPress, left, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                        QMouseEvent end(QEvent::MouseButtonRelease, right, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                        QApplication::sendEvent(alignedOverlay, &begin);
                        QApplication::sendEvent(alignedOverlay, &end);
                        okay &= alignedOverlay->selectedText() == QStringLiteral("left right");
                        const QPixmap rendered = alignedPin->grab();
                        const QImage pixels = rendered.toImage();
                        for (const QPointF& point : {left, right}) {
                            const QColor color = pixels.pixelColor((point * rendered.devicePixelRatio()).toPoint());
                            okay &= color.blue() > color.red() + 25;
                        }
                    }
                } else okay = false;
                qInfo() << "Pin OCR alignment: border" << border << "DPR" << dpr << okay;
                alignedPin->parentWidget()->close();
                QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            }
        }
        manager.setting()->setPinNoBorder(previousNoBorder);
        manager.setting()->setSelectionBorderColor(previousPinColor);
        manager.setting()->setPaddleOcrConfig(previousOcr);
        qInfo() << "Screenshot OCR pins automatically, preserves annotations, scans and cancels:" << okay;
        manager.destroy();
        app.exit(okay ? 0 : 2);
    });
    return app.exec();
}
