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

int runScreenshotSelectionTest(QApplication& app) {
    if (qEnvironmentVariableIsSet("NTSCREENSHOT_AUTO_SAVE_TEST"))
        return runScreenshotAutoSaveTest(app);
    app.setApplicationName(QStringLiteral("ntscreenshot-selection-test"));
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
