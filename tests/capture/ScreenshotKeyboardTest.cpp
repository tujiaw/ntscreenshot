#include "app/WindowManager.h"
#include "core/settings/SettingModel.h"
#include "modules/capture/screenshot/Screenshot.h"

#include <QApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QKeyEvent>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace {
// Model a focused child/popup that handles the key before QWidget propagation.
class KeySink : public QWidget {
public:
    using QWidget::QWidget;
protected:
    void keyPressEvent(QKeyEvent* event) override { event->accept(); }
};
}

int runScreenshotKeyboardTest(QApplication& app)
{
    app.setApplicationName(QStringLiteral("ntscreenshot-keyboard-test"));
    app.setQuitOnLastWindowClosed(false);
    WindowManager manager;
    const bool previousMode = manager.setting()->openCVMode();
    manager.setting()->setOpenCVMode(false);
    bool okay = true;
    auto check = [&](bool condition, const char* message) {
        if (!condition) qWarning() << "Screenshot keyboard regression:" << message;
        okay &= condition;
    };
    {
        ScreenshotWidget screenshot(&manager);
        int closeRequests = 0;
        QObject::connect(&screenshot, &ScreenshotWidget::sigClose, &screenshot, [&] { ++closeRequests; });
        auto press = [](QWidget* target, int key, bool repeat = false) {
            QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier, QString(), repeat);
            QApplication::sendEvent(target, &event);
        };
        auto release = [](QWidget* target, int key) {
            QKeyEvent event(QEvent::KeyRelease, key, Qt::NoModifier);
            QApplication::sendEvent(target, &event);
        };

        press(&screenshot, Qt::Key_Alt);
        check(manager.setting()->openCVMode(), "one ALT press switches to smart detection");
        for (int i = 0; i < 5; ++i) press(&screenshot, Qt::Key_Alt, true);
        check(manager.setting()->openCVMode(), "holding ALT must not repeatedly toggle detection");
        release(&screenshot, Qt::Key_Alt);

        KeySink child(&screenshot);
        child.show();
        KeySink popup(&screenshot, Qt::Tool | Qt::FramelessWindowHint);
        popup.show();
        // Real presses remain effective after many switches and focus changes.
        for (int i = 0; i < 20; ++i) {
            QWidget* target = i % 2 ? &child : &popup;
            const bool before = manager.setting()->openCVMode();
            press(target, Qt::Key_Alt);
            check(manager.setting()->openCVMode() != before, "ALT works with focused children and owned windows");
            release(target, Qt::Key_Alt);
        }
        QKeyEvent override(QEvent::ShortcutOverride, Qt::Key_Escape, Qt::NoModifier);
        override.ignore();
        QApplication::sendEvent(&child, &override);
        check(override.isAccepted(), "picking reserves ESC before child shortcuts run");

#ifdef Q_OS_WIN
        if (QGuiApplication::platformName() == QStringLiteral("windows")) {
            // Exercise the native ALT down/up path, including Windows' system
            // key handling, rather than only sending Qt key events.
            const HWND handle = reinterpret_cast<HWND>(screenshot.winId());
            for (int i = 0; i < 10; ++i) {
                const bool before = manager.setting()->openCVMode();
                PostMessageW(handle, WM_SYSKEYDOWN, VK_MENU, 1 | (0x38 << 16));
                PostMessageW(handle, WM_SYSKEYUP, VK_MENU,
                             LPARAM(1) | (LPARAM(0x38) << 16) | (LPARAM(1) << 29)
                             | (LPARAM(1) << 30) | (LPARAM(1) << 31));
                QElapsedTimer deadline;
                deadline.start();
                while (manager.setting()->openCVMode() == before && deadline.elapsed() < 1000)
                    app.processEvents(QEventLoop::AllEvents, 10);
                check(manager.setting()->openCVMode() != before, "native ALT presses keep switching modes");
            }
            PostMessageW(reinterpret_cast<HWND>(popup.winId()), WM_KEYDOWN, VK_ESCAPE, 1);
            PostMessageW(reinterpret_cast<HWND>(popup.winId()), WM_KEYUP, VK_ESCAPE,
                         LPARAM(1) | (LPARAM(1) << 30) | (LPARAM(1) << 31));
            QElapsedTimer deadline;
            deadline.start();
            while (closeRequests == 0 && deadline.elapsed() < 1000)
                app.processEvents(QEventLoop::AllEvents, 10);
            check(closeRequests == 1, "native ESC exits picking after repeated ALT presses");
            closeRequests = 0;
        }
#endif
        press(&child, Qt::Key_Escape);
        check(closeRequests == 1, "ESC exits picking even when a child consumes keys");
        press(&popup, Qt::Key_Escape);
        check(closeRequests == 2, "ESC exits picking from an owned tool window");

        KeySink unrelated;
        unrelated.show();
        const bool before = manager.setting()->openCVMode();
        press(&unrelated, Qt::Key_Alt);
        press(&unrelated, Qt::Key_Escape);
        check(manager.setting()->openCVMode() == before && closeRequests == 2,
              "unrelated windows keep their own keyboard handling");

        screenshot.hide();
        press(&child, Qt::Key_Escape);
        check(closeRequests == 2, "a hidden screenshot does not intercept keys");
    }
    manager.setting()->setOpenCVMode(previousMode);
    qInfo() << "Screenshot ALT/ESC keyboard regression:" << (okay ? "PASS" : "FAIL");
    return okay ? 0 : 1;
}
