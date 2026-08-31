#include "GlobalTextSelectionManager.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QMetaObject>
#include <QThread>
#include <QTimer>
#include <cstring>

#include "core/settings/SettingModel.h"
#include "modules/text_selection/TextSelectionToolbar.h"

#ifdef Q_OS_WIN
namespace {
GlobalTextSelectionManager *g_textSelectionManager = nullptr;

constexpr int kPassiveClipboardWaitMs = 80;
constexpr int kClipboardCopyWaitMs = 220;
constexpr int kRetryPauseScheduleMs[] = {0, 40, 120};

struct ClipboardTextSnapshot {
    bool hasText = false;
    QString text;
};

QString windowClassName(HWND hwnd)
{
    if (!hwnd) {
        return QString();
    }

    wchar_t buffer[256] = {0};
    const int len = ::GetClassNameW(hwnd, buffer, static_cast<int>(sizeof(buffer) / sizeof(buffer[0])));
    if (len <= 0) {
        return QString();
    }
    return QString::fromWCharArray(buffer, len);
}

QString processNameFromWindow(HWND hwnd)
{
    if (!hwnd) {
        return QString();
    }

    DWORD processId = 0;
    ::GetWindowThreadProcessId(hwnd, &processId);
    if (processId == 0) {
        return QString();
    }

    HANDLE processHandle = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!processHandle) {
        return QString();
    }

    wchar_t buffer[MAX_PATH] = {0};
    DWORD size = static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0]));
    QString processName;
    if (::QueryFullProcessImageNameW(processHandle, 0, buffer, &size) != FALSE) {
        processName = QFileInfo(QString::fromWCharArray(buffer, size)).fileName();
    }
    ::CloseHandle(processHandle);
    return processName;
}

QString windowTitle(HWND hwnd)
{
    if (!hwnd) {
        return QString();
    }

    const int len = ::GetWindowTextLengthW(hwnd);
    if (len <= 0) {
        return QString();
    }

    std::wstring buffer(static_cast<size_t>(len) + 1, L'\0');
    const int copied = ::GetWindowTextW(hwnd, buffer.data(), static_cast<int>(buffer.size()));
    if (copied <= 0) {
        return QString();
    }
    return QString::fromWCharArray(buffer.data(), copied);
}

bool containsKeyword(const QString &value, const QStringList &keywords)
{
    const QString normalizedValue = value.trimmed().toLower();
    if (normalizedValue.isEmpty()) {
        return false;
    }

    for (const QString &keyword : keywords) {
        if (normalizedValue.contains(keyword)) {
            return true;
        }
    }
    return false;
}

bool isVsCodeLikeTerminalWindow(HWND hwnd)
{
    static const QStringList kVsCodeProcessKeywords = {
        QStringLiteral("code"),
        QStringLiteral("code - insiders"),
        QStringLiteral("codium"),
        QStringLiteral("cursor"),
        QStringLiteral("windsurf"),
        QStringLiteral("trae")
    };
    static const QStringList kTerminalTitleKeywords = {
        QStringLiteral(" terminal"),
        QStringLiteral("zsh"),
        QStringLiteral("bash"),
        QStringLiteral("pwsh"),
        QStringLiteral("powershell"),
        QStringLiteral("cmd"),
        QStringLiteral("git bash"),
        QStringLiteral("wsl")
    };

    return containsKeyword(processNameFromWindow(hwnd), kVsCodeProcessKeywords)
        && containsKeyword(windowTitle(hwnd), kTerminalTitleKeywords);
}

bool isTerminalLikeWindow(HWND hwnd)
{
    static const QStringList kTerminalClassKeywords = {
        QStringLiteral("putty"),
        QStringLiteral("tmobaxtermform"),
        QStringLiteral("consolewindowclass"),
        QStringLiteral("cascadia_hosting_window_class"),
        QStringLiteral("virtualconsoleclass"),
        QStringLiteral("mintty")
    };
    static const QStringList kTerminalProcessKeywords = {
        QStringLiteral("putty"),
        QStringLiteral("mobaxterm"),
        QStringLiteral("windowsterminal"),
        QStringLiteral("conhost"),
        QStringLiteral("mintty"),
        QStringLiteral("conemu"),
        QStringLiteral("wezterm"),
        QStringLiteral("alacritty"),
        QStringLiteral("tabby"),
        QStringLiteral("xshell"),
        QStringLiteral("securecrt"),
        QStringLiteral("kitty")
    };

    return containsKeyword(windowClassName(hwnd), kTerminalClassKeywords)
        || containsKeyword(processNameFromWindow(hwnd), kTerminalProcessKeywords)
        || isVsCodeLikeTerminalWindow(hwnd);
}

bool waitForClipboardSequenceChange(DWORD beforeSequence, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        if (::GetClipboardSequenceNumber() != beforeSequence) {
            return true;
        }
        QThread::msleep(10);
    }
    return ::GetClipboardSequenceNumber() != beforeSequence;
}

bool openClipboardWithRetry(HWND owner = nullptr, int attempts = 6, DWORD delayMs = 8)
{
    for (int i = 0; i < attempts; ++i) {
        if (::OpenClipboard(owner) != FALSE) {
            return true;
        }
        ::Sleep(delayMs);
    }
    return false;
}

ClipboardTextSnapshot snapshotClipboardText()
{
    ClipboardTextSnapshot snapshot;
    if (!openClipboardWithRetry()) {
        return snapshot;
    }

    if (::IsClipboardFormatAvailable(CF_UNICODETEXT) == FALSE) {
        ::CloseClipboard();
        return snapshot;
    }

    HANDLE clipboardData = ::GetClipboardData(CF_UNICODETEXT);
    if (!clipboardData) {
        ::CloseClipboard();
        return snapshot;
    }

    const auto *lockedText = static_cast<const wchar_t*>(::GlobalLock(clipboardData));
    if (lockedText) {
        snapshot.hasText = true;
        snapshot.text = QString::fromWCharArray(lockedText).trimmed();
        ::GlobalUnlock(clipboardData);
    }

    ::CloseClipboard();
    return snapshot;
}

bool setClipboardPlainText(const QString &text)
{
    if (!openClipboardWithRetry()) {
        return false;
    }

    if (::EmptyClipboard() == FALSE) {
        ::CloseClipboard();
        return false;
    }

    const std::wstring wideText = text.toStdWString();
    const size_t bytes = (wideText.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!memory) {
        ::CloseClipboard();
        return false;
    }

    void *buffer = ::GlobalLock(memory);
    if (!buffer) {
        ::GlobalFree(memory);
        ::CloseClipboard();
        return false;
    }

    memcpy(buffer, wideText.c_str(), bytes);
    ::GlobalUnlock(memory);

    if (::SetClipboardData(CF_UNICODETEXT, memory) == nullptr) {
        ::GlobalFree(memory);
        ::CloseClipboard();
        return false;
    }

    ::CloseClipboard();
    return true;
}

QString clipboardTextOrEmpty()
{
    ClipboardTextSnapshot snapshot = snapshotClipboardText();
    QString text = snapshot.text;
    if (text.length() > 5000) {
        text = text.left(5000);
    }
    return text;
}

bool isMeaningfulText(const QString &text)
{
    for (const QChar ch : text) {
        const QChar::Category cat = ch.category();
        if (cat == QChar::Letter_Uppercase
            || cat == QChar::Letter_Lowercase
            || cat == QChar::Letter_Titlecase
            || cat == QChar::Letter_Modifier
            || cat == QChar::Letter_Other
            || cat == QChar::Number_DecimalDigit
            || cat == QChar::Number_Letter
            || cat == QChar::Number_Other) {
            return true;
        }
    }
    return false;
}

QString normalizeSelectedText(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    return text.trimmed();
}

void waitBriefly(int delayMs)
{
    if (delayMs <= 0) {
        return;
    }

    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < delayMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(5);
    }
}
}
#endif

GlobalTextSelectionManager::GlobalTextSelectionManager(QObject *parent)
    : QObject(parent)
    , popup_(std::make_unique<TextSelectionToolbar>(nullptr))
    , setting_(nullptr)
{
    connect(popup_.get(), &TextSelectionToolbar::sigActionTriggered, this, [this](const QString &actionId, const QString &inputText) {
        if (actionId == QStringLiteral("copy")) {
            if (QClipboard *clipboard = QApplication::clipboard()) {
                clipboard->setText(selectedText_, QClipboard::Clipboard);
            }
            hidePopup();
            return;
        }
        emit sigActionTriggered(actionId, selectedText_, inputText);
        hidePopup();
    });
    popup_->hide();

#ifdef Q_OS_WIN
    installMouseHook();
#endif
}

GlobalTextSelectionManager::~GlobalTextSelectionManager()
{
#ifdef Q_OS_WIN
    uninstallMouseHook();
#endif
}

void GlobalTextSelectionManager::setSettingModel(SettingModel *setting)
{
    setting_ = setting;
}

#ifdef Q_OS_WIN
LRESULT CALLBACK GlobalTextSelectionManager::mouseHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && g_textSelectionManager) {
        const auto *info = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        if (info) {
            const QPoint globalPos(info->pt.x, info->pt.y);
            QMetaObject::invokeMethod(
                g_textSelectionManager,
                [wParam, globalPos]() {
                    if (g_textSelectionManager) {
                        g_textSelectionManager->handleGlobalMouseEvent(wParam, globalPos);
                    }
                },
                Qt::QueuedConnection);
        }
    }

    return ::CallNextHookEx(nullptr, nCode, wParam, lParam);
}

void GlobalTextSelectionManager::handleGlobalMouseEvent(WPARAM wParam, const QPoint &globalPos)
{
    if (setting_ && !setting_->textSelectionEnabled()) {
        return;
    }

    if (wParam == WM_MOUSEWHEEL || wParam == WM_MOUSEHWHEEL) {
        hidePopup();
        return;
    }

    if (wParam == WM_RBUTTONDOWN || wParam == WM_MBUTTONDOWN) {
        if (!popupContainsGlobalPoint(globalPos)) {
            hidePopup();
        }
        return;
    }

    if (wParam == WM_LBUTTONDOWN) {
        if (popupContainsGlobalPoint(globalPos)) {
            return;
        }

        hidePopup();

        const DWORD now = ::GetTickCount();
        const QPoint delta = globalPos - lastClickPoint_;
        doubleClickCandidate_ = (now - lastClickTime_ <= ::GetDoubleClickTime())
            && (qAbs(delta.x()) <= ::GetSystemMetrics(SM_CXDOUBLECLK) / 2)
            && (qAbs(delta.y()) <= ::GetSystemMetrics(SM_CYDOUBLECLK) / 2);
        lastClickTime_ = now;
        lastClickPoint_ = globalPos;
        pressPoint_ = globalPos;
        dragging_ = false;
        dragSourceWindow_ = ::GetForegroundWindow();
        return;
    }

    if (wParam == WM_MOUSEMOVE) {
        if ((::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0
            && (globalPos - pressPoint_).manhattanLength() > QApplication::startDragDistance()) {
            dragging_ = true;
        }
        return;
    }

    if (wParam == WM_LBUTTONUP) {
        if (popupContainsGlobalPoint(globalPos)) {
            doubleClickCandidate_ = false;
            dragging_ = false;
            return;
        }

        doubleClickCandidate_ = false; // 强制关闭，双击鼠标不触发
        const bool shouldTrigger = dragging_ || doubleClickCandidate_;
        const HWND sourceWindow = dragSourceWindow_;
        dragging_ = false;
        doubleClickCandidate_ = false;
        dragSourceWindow_ = nullptr;

        if (!shouldTrigger) {
            return;
        }

        const int delayMs = 60;
        QTimer::singleShot(delayMs, this, [this, globalPos, sourceWindow]() {
            triggerForSelection(globalPos, sourceWindow);
        });
    }
}

void GlobalTextSelectionManager::triggerForSelection(const QPoint &globalPos, HWND sourceWindow)
{
    const QString text = captureSelectedTextWithRetry(sourceWindow ? sourceWindow : ::GetForegroundWindow());
    if (!isMeaningfulText(text)) {
        return;
    }

    showPopup(globalPos, text);
}

bool GlobalTextSelectionManager::installMouseHook()
{
    if (mouseHook_) {
        return true;
    }

    g_textSelectionManager = this;
    mouseHook_ = ::SetWindowsHookExW(WH_MOUSE_LL, &GlobalTextSelectionManager::mouseHookProc,
                                     ::GetModuleHandleW(nullptr), 0);
    if (!mouseHook_) {
        g_textSelectionManager = nullptr;
        return false;
    }
    return true;
}

void GlobalTextSelectionManager::uninstallMouseHook()
{
    if (mouseHook_) {
        ::UnhookWindowsHookEx(mouseHook_);
        mouseHook_ = nullptr;
    }

    if (g_textSelectionManager == this) {
        g_textSelectionManager = nullptr;
    }
}

bool GlobalTextSelectionManager::popupContainsGlobalPoint(const QPoint &globalPos) const
{
    return popup_ && popup_->isVisible() && popup_->containsGlobalPoint(globalPos);
}

bool GlobalTextSelectionManager::isOwnProcessWindow(HWND hwnd) const
{
    if (!hwnd) {
        return true;
    }

    DWORD processId = 0;
    ::GetWindowThreadProcessId(hwnd, &processId);
    return processId == ::GetCurrentProcessId();
}

QString GlobalTextSelectionManager::captureSelectedText(HWND sourceWindow)
{
    if (!sourceWindow || !::IsWindow(sourceWindow) || isOwnProcessWindow(sourceWindow)) {
        return QString();
    }

    const ClipboardTextSnapshot backup = snapshotClipboardText();
    const DWORD beforeSequence = ::GetClipboardSequenceNumber();
    const bool terminalLikeWindow = isTerminalLikeWindow(sourceWindow);
    bool clipboardChanged = false;

    if (waitForClipboardSequenceChange(beforeSequence, terminalLikeWindow ? 150 : kPassiveClipboardWaitMs)) {
        clipboardChanged = true;
        const QString result = normalizeSelectedText(clipboardTextOrEmpty());
        if (backup.hasText && result != backup.text) {
            setClipboardPlainText(backup.text);
        }
        return result;
    }

    if (terminalLikeWindow) {
        return QString();
    }

    ::SetForegroundWindow(sourceWindow);
    INPUT inputs[4];
    ZeroMemory(inputs, sizeof(inputs));
    inputs[0].type = INPUT_KEYBOARD; inputs[0].ki.wVk = VK_CONTROL;
    inputs[1].type = INPUT_KEYBOARD; inputs[1].ki.wVk = 'C';
    inputs[2].type = INPUT_KEYBOARD; inputs[2].ki.wVk = 'C'; inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[3].type = INPUT_KEYBOARD; inputs[3].ki.wVk = VK_CONTROL; inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    ::SendInput(4, inputs, sizeof(INPUT));

    clipboardChanged = waitForClipboardSequenceChange(beforeSequence, kClipboardCopyWaitMs);

    QString result;
    if (clipboardChanged) {
        result = normalizeSelectedText(clipboardTextOrEmpty());
    }

    if (clipboardChanged && backup.hasText && result != backup.text) {
        setClipboardPlainText(backup.text);
    }
    return result;
}

QString GlobalTextSelectionManager::captureSelectedTextWithRetry(HWND sourceWindow)
{
    QString bestEffortText;

    for (int delayMs : kRetryPauseScheduleMs) {
        waitBriefly(delayMs);

        HWND activeWindow = ::GetForegroundWindow();
        if (activeWindow && ::IsWindow(activeWindow) && !isOwnProcessWindow(activeWindow)) {
            sourceWindow = activeWindow;
        }

        const QString text = captureSelectedText(sourceWindow);
        if (text.isEmpty()) {
            continue;
        }

        bestEffortText = text;
        if (isMeaningfulText(text)) {
            return text;
        }
    }

    return bestEffortText;
}
#endif

void GlobalTextSelectionManager::hidePopup()
{
    if (popup_) {
        popup_->hide();
    }
}

void GlobalTextSelectionManager::showPopup(const QPoint &globalPos, const QString &selectedText)
{
    selectedText_ = selectedText;
    if (popup_) {
        popup_->hide();
        popup_->setActions(setting_ ? setting_->textSelectionActions()
                                    : QList<TextSelectionActionConfig>());
        popup_->showNearGlobalPoint(globalPos);
    }
}
