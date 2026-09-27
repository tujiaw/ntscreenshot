#include "GlobalTextSelectionManager.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QMetaObject>
#include <QPointer>
#include <QThread>
#include <QTimer>
#include <QtConcurrent>
#include <cstring>

#ifdef Q_OS_WIN
#include <UIAutomation.h>
#include <ole2.h>
#endif

#include "core/settings/SettingModel.h"
#include "modules/text_selection/TextSelectionToolbar.h"

#ifdef Q_OS_WIN
namespace {
GlobalTextSelectionManager *g_textSelectionManager = nullptr;

constexpr int kMaxSelectedText = 5000;
constexpr int kClipboardCopyWaitMs = 220;

template<typename T> struct ComHolder {
    T *ptr = nullptr;
    ~ComHolder() { if (ptr) ptr->Release(); }
    T **put() { return &ptr; }
    T *operator->() const { return ptr; }
    explicit operator bool() const { return ptr != nullptr; }
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

bool sourceIsCurrent(HWND source)
{
    return source && ::IsWindow(source)
        && ::GetAncestor(::GetForegroundWindow(), GA_ROOT) == ::GetAncestor(source, GA_ROOT);
}

TextSelectionResult readClipboardSelection(HWND source)
{
    TextSelectionResult result;
    if (!sourceIsCurrent(source) || isTerminalLikeWindow(source)) return result;

    // Keep the complete OLE data object, not just CF_UNICODETEXT. This preserves
    // images, HTML, files and delayed-rendered clipboard formats while Ctrl+C runs.
    const bool comInitialized = SUCCEEDED(::CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    ComHolder<IDataObject> previousClipboard;
    const bool hadPreviousClipboard = SUCCEEDED(::OleGetClipboard(previousClipboard.put()));

    const DWORD before = ::GetClipboardSequenceNumber();
    INPUT inputs[4] = {};
    inputs[0].type = INPUT_KEYBOARD; inputs[0].ki.wVk = VK_CONTROL;
    inputs[1].type = INPUT_KEYBOARD; inputs[1].ki.wVk = 'C';
    inputs[2].type = INPUT_KEYBOARD; inputs[2].ki.wVk = 'C'; inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[3].type = INPUT_KEYBOARD; inputs[3].ki.wVk = VK_CONTROL; inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    if (::SendInput(4, inputs, sizeof(INPUT)) != 4) {
        if (comInitialized) ::CoUninitialize();
        return result;
    }

    for (int elapsed = 0; elapsed < kClipboardCopyWaitMs; elapsed += 10) {
        if (::GetClipboardSequenceNumber() != before) break;
        QThread::msleep(10);
    }
    if (::GetClipboardSequenceNumber() == before) {
        if (comInitialized) ::CoUninitialize();
        return result;
    }

    if (!::OpenClipboard(nullptr)) {
        if (comInitialized) ::CoUninitialize();
        return result;
    }
    const DWORD copiedSequence = ::GetClipboardSequenceNumber();
    if (::IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        if (HANDLE data = ::GetClipboardData(CF_UNICODETEXT)) {
            if (const auto *chars = static_cast<const wchar_t*>(::GlobalLock(data))) {
                result.text = QString::fromWCharArray(chars, kMaxSelectedText + 1);
                ::GlobalUnlock(data);
            }
        }
    }
    ::CloseClipboard();
    // Restore only if no other application changed the clipboard after our copy.
    if (::GetClipboardSequenceNumber() == copiedSequence) {
        if (hadPreviousClipboard) {
            ::OleSetClipboard(previousClipboard.ptr);
        } else if (::OpenClipboard(nullptr)) {
            ::EmptyClipboard();
            ::CloseClipboard();
        }
    }
    if (comInitialized) ::CoUninitialize();
    if (!sourceIsCurrent(source)) return {};
    result.truncated = result.text.size() > kMaxSelectedText;
    result.text = result.text.left(kMaxSelectedText);
    return result;
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

TextSelectionResult readUiAutomationSelection(const QPoint &point, bool &supported)
{
    TextSelectionResult result;
    supported = false;
    if (FAILED(::CoInitializeEx(nullptr, COINIT_MULTITHREADED))) return result;
    {
        ComHolder<IUIAutomation> automation;
        if (SUCCEEDED(::CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                                         IID_IUIAutomation, reinterpret_cast<void**>(automation.put())))) {
            ComHolder<IUIAutomation2> automation2;
            if (SUCCEEDED(automation->QueryInterface(IID_IUIAutomation2,
                                                      reinterpret_cast<void**>(automation2.put())))) {
                automation2->put_ConnectionTimeout(250);
                automation2->put_TransactionTimeout(250);
            }
            ComHolder<IUIAutomationElement> element;
            POINT nativePoint{point.x(), point.y()};
            if (SUCCEEDED(automation->ElementFromPoint(nativePoint, element.put())) && element) {
                ComHolder<IUIAutomationTextPattern> pattern;
                if (SUCCEEDED(element->GetCurrentPatternAs(UIA_TextPatternId, IID_IUIAutomationTextPattern,
                              reinterpret_cast<void**>(pattern.put()))) && pattern) {
                    supported = true;
                    ComHolder<IUIAutomationTextRangeArray> ranges;
                    if (SUCCEEDED(pattern->GetSelection(ranges.put())) && ranges) {
                        int count = 0;
                        ranges->get_Length(&count);
                        if (count == 1) {
                            ComHolder<IUIAutomationTextRange> range;
                            if (SUCCEEDED(ranges->GetElement(0, range.put())) && range) {
                                BSTR value = nullptr;
                                if (SUCCEEDED(range->GetText(kMaxSelectedText + 1, &value)) && value) {
                                    result.text = QString::fromWCharArray(value, ::SysStringLen(value));
                                    ::SysFreeString(value);
                                    result.truncated = result.text.size() > kMaxSelectedText;
                                    result.text.truncate(kMaxSelectedText);
                                }
                                SAFEARRAY *rectangles = nullptr;
                                if (SUCCEEDED(range->GetBoundingRectangles(&rectangles)) && rectangles) {
                                    double *values = nullptr;
                                    LONG lower = 0, upper = -1;
                                    ::SafeArrayGetLBound(rectangles, 1, &lower);
                                    ::SafeArrayGetUBound(rectangles, 1, &upper);
                                    if (upper - lower + 1 >= 4 && SUCCEEDED(::SafeArrayAccessData(rectangles, reinterpret_cast<void**>(&values)))) {
                                        for (LONG i = 0; i + 3 < upper - lower + 1; i += 4) {
                                            const QRect part(static_cast<int>(values[i]), static_cast<int>(values[i + 1]),
                                                             static_cast<int>(values[i + 2]), static_cast<int>(values[i + 3]));
                                            result.bounds = result.bounds.isValid() ? result.bounds.united(part) : part;
                                        }
                                        ::SafeArrayUnaccessData(rectangles);
                                    }
                                    ::SafeArrayDestroy(rectangles);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    ::CoUninitialize();
    return result;
}

TextSelectionResult readSelection(HWND source, const QPoint &point)
{
    if (!sourceIsCurrent(source)) return {};
    bool supported = false;
    TextSelectionResult result = readUiAutomationSelection(point, supported);
    if (!sourceIsCurrent(source)) return {};
    // Some providers advertise TextPattern but return an empty/degenerate
    // selection at the release point. In that case use the clipboard-preserving
    // fallback instead of silently dropping an otherwise valid selection.
    if (!supported || result.text.trimmed().isEmpty()) result = readClipboardSelection(source);
    result.text = normalizeSelectedText(result.text);
    if (!isMeaningfulText(result.text)) return {};
    return result;
}
}
#endif

GlobalTextSelectionManager::GlobalTextSelectionManager(QObject *parent)
    : QObject(parent)
    , popup_(std::make_unique<TextSelectionToolbar>(nullptr))
    , setting_(nullptr)
{
    connect(popup_.get(), &TextSelectionToolbar::sigActionTriggered, this, [this](const QString &actionId, const QString &inputText) {
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
        if (info && (info->flags & LLMHF_INJECTED) == 0
            && (wParam == WM_LBUTTONDOWN || wParam == WM_LBUTTONUP
                || (wParam == WM_MOUSEMOVE && (::GetAsyncKeyState(VK_LBUTTON) & 0x8000))
                || wParam == WM_RBUTTONDOWN || wParam == WM_MBUTTONDOWN
                || wParam == WM_MOUSEWHEEL || wParam == WM_MOUSEHWHEEL)) {
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
        ++generation_;
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
            dragging_ = false;
            return;
        }

        const bool shouldTrigger = dragging_;
        const HWND sourceWindow = dragSourceWindow_;
        dragging_ = false;
        dragSourceWindow_ = nullptr;

        if (!shouldTrigger) {
            return;
        }

        const uint64_t generation = generation_;
        QTimer::singleShot(60, this, [this, globalPos, sourceWindow, generation]() {
            triggerForSelection(globalPos, sourceWindow, generation);
        });
    }
}

void GlobalTextSelectionManager::triggerForSelection(const QPoint &globalPos, HWND sourceWindow, uint64_t generation)
{
    if (generation != generation_ || activeReads_ >= 2 || isOwnProcessWindow(sourceWindow)
        || !sourceIsCurrent(sourceWindow)
        || (setting_ && !setting_->textSelectionEnabled())) return;
    ++activeReads_;
    auto *watcher = new QFutureWatcher<TextSelectionResult>(this);
    connect(watcher, &QFutureWatcher<TextSelectionResult>::finished, this,
            [this, watcher, globalPos, sourceWindow, generation]() {
        --activeReads_;
        const TextSelectionResult result = watcher->result();
        watcher->deleteLater();
        if (generation == generation_ && sourceIsCurrent(sourceWindow)
            && (!setting_ || setting_->textSelectionEnabled()) && !result.text.isEmpty()) {
            showPopup(globalPos, result);
        }
    });
    watcher->setFuture(QtConcurrent::run([sourceWindow, globalPos]() {
        return readSelection(sourceWindow, globalPos);
    }));
    QTimer::singleShot(700, watcher, [this, generation]() {
        if (generation == generation_) ++generation_;
    });
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

#endif

void GlobalTextSelectionManager::hidePopup()
{
    if (popup_) {
        popup_->hide();
    }
}

void GlobalTextSelectionManager::showPopup(const QPoint &globalPos, const TextSelectionResult &result)
{
    selectedText_ = result.text;
    if (popup_) {
        popup_->hide();
        popup_->setActions(setting_ ? setting_->textSelectionActions()
                                    : QList<TextSelectionActionConfig>());
        popup_->showNearGlobalPoint(globalPos, result.bounds);
    }
}
