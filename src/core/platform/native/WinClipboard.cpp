#include "WinClipboard.h"

#include "core/imaging/ImageUtil.h"

#include <QApplication>
#include <QClipboard>

namespace WinClipboard {

namespace {

void PushKey(INPUT* inputs, UINT& count, WORD virtualKey, DWORD flags = 0) {
    inputs[count].type = INPUT_KEYBOARD;
    inputs[count].ki.wVk = virtualKey;
    inputs[count].ki.wScan = 0;
    inputs[count].ki.dwFlags = flags;
    inputs[count].ki.time = 0;
    inputs[count].ki.dwExtraInfo = 0;
    ++count;
}

bool KeyIsDown(int virtualKey) {
    return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
}

} // namespace

QString ReadText() {
    if (!OpenClipboard(nullptr)) {
        return {};
    }
    QString text;
    const HANDLE handle = GetClipboardData(CF_UNICODETEXT);
    if (handle) {
        const auto* data = static_cast<const wchar_t*>(GlobalLock(handle));
        if (data) {
            text = QString::fromWCharArray(data);
            GlobalUnlock(handle);
        }
    }
    CloseClipboard();
    return text;
}

QImage ReadImage() {
    QClipboard* clipboard = QApplication::clipboard();
    if (!clipboard) {
        return {};
    }
    const QImage image = clipboard->image();
    return image;
}

bool SetText(const QString& text) {
    QClipboard* clipboard = QApplication::clipboard();
    if (!clipboard) {
        return false;
    }
    clipboard->setText(text);
    return true;
}

bool SetImage(const QImage& image, int maxEdge) {
    QClipboard* clipboard = QApplication::clipboard();
    if (!clipboard || image.isNull()) {
        return false;
    }
    clipboard->setImage(ImageUtil::ScaleToMaxEdge(image, maxEdge));
    return true;
}

void SendShiftInsert() {
    INPUT inputs[4] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_SHIFT;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = VK_INSERT;
    inputs[1].ki.dwFlags = KEYEVENTF_EXTENDEDKEY;
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = VK_INSERT;
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP | KEYEVENTF_EXTENDEDKEY;
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_SHIFT;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, inputs, sizeof(INPUT));
}

void SendPasteForHotkey(UINT) {
    const bool ctrlDown = KeyIsDown(VK_CONTROL);
    const bool altDown = KeyIsDown(VK_MENU);
    const bool shiftDown = KeyIsDown(VK_SHIFT);

    INPUT inputs[12] = {};
    UINT count = 0;

    if (altDown) {
        PushKey(inputs, count, VK_MENU, KEYEVENTF_KEYUP);
    }
    if (ctrlDown) {
        PushKey(inputs, count, VK_CONTROL, KEYEVENTF_KEYUP);
    }
    if (!shiftDown) {
        PushKey(inputs, count, VK_SHIFT);
    }

    PushKey(inputs, count, VK_INSERT, KEYEVENTF_EXTENDEDKEY);
    PushKey(inputs, count, VK_INSERT, KEYEVENTF_KEYUP | KEYEVENTF_EXTENDEDKEY);

    if (!shiftDown) {
        PushKey(inputs, count, VK_SHIFT, KEYEVENTF_KEYUP);
    }
    if (ctrlDown) {
        PushKey(inputs, count, VK_CONTROL);
    }
    if (altDown) {
        PushKey(inputs, count, VK_MENU);
    }

    if (count > 0) {
        SendInput(count, inputs, sizeof(INPUT));
    }
}

void RestoreFocus(HWND targetWindow, HWND targetFocusWindow) {
    if (!targetWindow || !::IsWindow(targetWindow)) {
        return;
    }

    const DWORD currentThread = GetCurrentThreadId();
    const DWORD targetThread = GetWindowThreadProcessId(targetWindow, nullptr);
    const DWORD foregroundThread = GetWindowThreadProcessId(::GetForegroundWindow(), nullptr);

    if (targetThread) {
        AttachThreadInput(currentThread, targetThread, TRUE);
    }
    if (foregroundThread && foregroundThread != targetThread) {
        AttachThreadInput(currentThread, foregroundThread, TRUE);
    }

    ::SetForegroundWindow(targetWindow);
    ::BringWindowToTop(targetWindow);
    if (targetFocusWindow && ::IsWindow(targetFocusWindow)) {
        ::SetFocus(targetFocusWindow);
    }

    if (foregroundThread && foregroundThread != targetThread) {
        AttachThreadInput(currentThread, foregroundThread, FALSE);
    }
    if (targetThread) {
        AttachThreadInput(currentThread, targetThread, FALSE);
    }
}

HWND ForegroundExternalWindow() {
    const HWND hwnd = ::GetForegroundWindow();
    if (!hwnd) {
        return nullptr;
    }
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId == GetCurrentProcessId()) {
        return nullptr;
    }
    return hwnd;
}

HWND FocusWindowOf(HWND targetWindow) {
    if (!targetWindow || !::IsWindow(targetWindow)) {
        return nullptr;
    }
    const DWORD targetThread = GetWindowThreadProcessId(targetWindow, nullptr);
    if (targetThread == 0) {
        return nullptr;
    }
    GUITHREADINFO info{};
    info.cbSize = sizeof(info);
    if (!GetGUIThreadInfo(targetThread, &info) || !info.hwndFocus || !::IsWindow(info.hwndFocus)) {
        return nullptr;
    }
    return info.hwndFocus;
}

bool IsOwnWindow(HWND window, HWND primaryWindow, HWND secondaryWindow) {
    if (!window) {
        return false;
    }
    if (window == primaryWindow) {
        return true;
    }
    if (secondaryWindow &&
        (window == secondaryWindow || ::IsChild(secondaryWindow, window))) {
        return true;
    }
    return false;
}

DWORD ClipboardSequence() {
    return GetClipboardSequenceNumber();
}

} // namespace WinClipboard
