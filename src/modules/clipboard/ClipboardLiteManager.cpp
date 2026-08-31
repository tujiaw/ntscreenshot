#include "ClipboardLiteManager.h"

#include "AiFillSettingsDialog.h"
#include "HotkeyOptions.h"
#include "core/imaging/ImageUtil.h"
#include "core/platform/Util.h"
#include "core/platform/native/WinClipboard.h"
#include "core/theme/MenuCheckMark.h"

#include <QBuffer>
#include <QClipboard>
#include <QCursor>
#include <QDebug>
#include <QIcon>
#include <QImage>
#include <QMenu>
#include <QPixmap>
#include <QTimer>

#include <limits>

namespace {

QByteArray ImageToPng(const QImage& image) {
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG")) {
        return {};
    }
    return bytes;
}

constexpr int kMaxHistoryValues[] = {20, 50, 100, 200};

} // namespace

void ClipboardLiteManager::initialize()
{
    qInfo() << "ClipboardLiteManager::initialize: start";
    if (initialized_) {
        qInfo() << "ClipboardLiteManager::initialize: already initialized";
        return;
    }
    initialized_ = true;
    setObjectName(QStringLiteral("ClipboardLiteManager"));
    hide();

    pasteTimer_ = new QTimer(this);
    pasteTimer_->setSingleShot(true);
    connect(pasteTimer_, &QTimer::timeout, this, &ClipboardLiteManager::onPasteTimer);

    retryCaptureTimer_ = new QTimer(this);
    retryCaptureTimer_->setSingleShot(true);
    connect(retryCaptureTimer_, &QTimer::timeout, this, &ClipboardLiteManager::onRetryCaptureTimer);

    pollTimer_ = new QTimer(this);
    pollTimer_->setInterval(300);
    connect(pollTimer_, &QTimer::timeout, this, &ClipboardLiteManager::onPollTimer);

    trackTimer_ = new QTimer(this);
    trackTimer_->setInterval(120);
    connect(trackTimer_, &QTimer::timeout, this, &ClipboardLiteManager::onTrackTimer);

    syncTimer_ = new QTimer(this);
    syncTimer_->setInterval(5 * 60 * 1000);
    connect(syncTimer_, &QTimer::timeout, this, &ClipboardLiteManager::onSyncTimer);

    AddClipboardFormatListener(reinterpret_cast<HWND>(winId()));
    LoadSettings();
    SaveSettings();
    LoadStoredHistory();
    lastClipboardSequence_ = WinClipboard::ClipboardSequence();
    pollTimer_->start();
    trackTimer_->start();
    syncTimer_->start();
    RegisterShowHotkeyWithFallback();
    RegisterPasteHotkeysWithFallback();
    UpdateTrayTip();
    qInfo() << "ClipboardLiteManager::initialize: done";
}

void ClipboardLiteManager::shutdown()
{
    qInfo() << "ClipboardLiteManager::shutdown";
    if (!initialized_) {
        return;
    }
    SyncHistoryStore();
    RemoveClipboardFormatListener(reinterpret_cast<HWND>(winId()));
    for (int id = cl::id::HotkeyShowHistory; id <= cl::id::HotkeyPasteLast; ++id) {
        UnregisterHotKey(reinterpret_cast<HWND>(winId()), id);
    }
    if (popup_) {
        popup_->deleteLater();
        popup_ = nullptr;
    }
}

bool ClipboardLiteManager::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
    MSG* msg = reinterpret_cast<MSG*>(message);
    if (msg->message == WM_CLIPBOARDUPDATE) {
        if (!captureEnabled_) {
            lastClipboardSequence_ = WinClipboard::ClipboardSequence();
            if (result) {
                *result = 0;
            }
            return true;
        }
        retryCaptureCount_ = 0;
        CaptureClipboardContent();
        if (result) {
            *result = 0;
        }
        return true;
    }
    if (msg->message == WM_HOTKEY) {
        const int id = static_cast<int>(msg->wParam);
        if (id == cl::id::HotkeyShowHistory) {
            if (popup_ && popup_->IsPopupVisible()) {
                popup_->HidePopup();
            } else {
                CaptureTargetFromForeground();
                ShowHistory(true);
            }
        } else if (id >= cl::id::HotkeyPasteFirst && id <= cl::id::HotkeyPasteLast) {
            CaptureTargetFromForeground();
            const int idx = id - cl::id::HotkeyPasteFirst;
            const UINT triggerKey = static_cast<UINT>('1' + idx);
            BeginPaste(static_cast<size_t>(idx), true,
                       RegisteredPasteHotkeyModifiers(), triggerKey);
        }
        if (result) {
            *result = 0;
        }
        return true;
    }
    return QWidget::nativeEvent(eventType, message, result);
}

// ── Clipboard capture ──────────────────────────────────────────────

ClipboardLiteManager::CaptureResult ClipboardLiteManager::CaptureClipboardContent() {
    if (!captureEnabled_) {
        return CaptureResult::IgnoredOwnChange;
    }

    lastClipboardSequence_ = WinClipboard::ClipboardSequence();

    CaptureResult result = CaptureClipboardImage();
    if (result == CaptureResult::Failed) {
        result = CaptureClipboardText();
    }
    if (result == CaptureResult::Captured) {
        RefreshPopupIfVisible();
    }
    return result;
}

ClipboardLiteManager::CaptureResult ClipboardLiteManager::CaptureClipboardText() {
    QString text = WinClipboard::ReadText();
    if (text.isNull() || text.isEmpty()) {
        return CaptureResult::Failed;
    }
    ClipboardHistory::NormalizeText(text);

    ClipItem item;
    item.kind = ClipKind::Text;
    item.text = text;
    item.capturedAt = QDateTime::currentDateTime();
    if (ConsumeOwnClipboardChange(item)) {
        return CaptureResult::IgnoredOwnChange;
    }
    suppressNextCapture_ = false;
    if (history_.AddText(std::move(text))) {
        MarkHistoryDirty();
    }
    return CaptureResult::Captured;
}

ClipboardLiteManager::CaptureResult ClipboardLiteManager::CaptureClipboardImage() {
    QImage image = WinClipboard::ReadImage();
    if (image.isNull()) {
        return CaptureResult::Failed;
    }

    const quint64 pixelHash = ImageUtil::PixelHash(image);
    QByteArray png = ImageToPng(image);
    if (png.isEmpty()) {
        return CaptureResult::Failed;
    }

    ClipItem item;
    item.kind = ClipKind::Image;
    item.data = png;
    item.width = image.width();
    item.height = image.height();
    item.pixelHash = pixelHash;
    item.capturedAt = QDateTime::currentDateTime();
    if (ConsumeOwnClipboardChange(item)) {
        return CaptureResult::IgnoredOwnChange;
    }
    suppressNextCapture_ = false;
    if (history_.AddImage(png, image.width(), image.height(), pixelHash)) {
        MarkHistoryDirty();
    }
    return CaptureResult::Captured;
}

bool ClipboardLiteManager::ConsumeOwnClipboardChange(const ClipItem& item) {
    if (!suppressNextCapture_) {
        return false;
    }

    const DWORD currentSequence = WinClipboard::ClipboardSequence();
    const bool isPendingWrite = pendingClipboardSequence_ != 0 &&
                                currentSequence == pendingClipboardSequence_;
    const bool isPendingContent = pendingClipboardSequence_ == 0 &&
                                  ClipboardHistory::SameContent(item, pendingClipboardItem_);
    if (isPendingWrite || isPendingContent) {
        suppressNextCapture_ = false;
        pendingClipboardSequence_ = 0;
        return true;
    }
    suppressNextCapture_ = false;
    pendingClipboardSequence_ = 0;
    return false;
}

void ClipboardLiteManager::CaptureTargetFromForeground() {
    HWND foreground = ::GetForegroundWindow();
    if (!foreground ||
        WinClipboard::IsOwnWindow(foreground, reinterpret_cast<HWND>(winId()),
                                  popup_ ? reinterpret_cast<HWND>(popup_->winId()) : nullptr)) {
        return;
    }
    targetWindow_ = foreground;
    targetFocusWindow_ = WinClipboard::FocusWindowOf(foreground);
}

// ── Paste ─────────────────────────────────────────────────────────

void ClipboardLiteManager::BeginPaste(size_t index, bool keepPopupReady,
                                       UINT releaseModifiers, UINT triggerKey) {
    const auto& items = history_.Items();
    if (index >= items.size()) {
        return;
    }

    pendingClipboardItem_ = items[index];
    suppressNextCapture_ = true;
    pendingClipboardSequence_ = 0;
    if (!SetClipboardItem(pendingClipboardItem_)) {
        suppressNextCapture_ = false;
        return;
    }
    pendingClipboardSequence_ = WinClipboard::ClipboardSequence();

    pendingPasteReady_ = true;
    pendingKeepPopupVisible_ = keepPopupReady;
    pendingPasteReleaseModifiers_ = releaseModifiers;
    pendingPasteTriggerKey_ = triggerKey;
    pasteTimer_->start(triggerKey != 0 ? 15 : (keepPopupReady ? 180 : 400));
}

void ClipboardLiteManager::RestoreTargetFocus() {
    WinClipboard::RestoreFocus(targetWindow_, targetFocusWindow_);
}

bool ClipboardLiteManager::SetClipboardItem(const ClipItem& item) {
    if (item.kind == ClipKind::Text) {
        return WinClipboard::SetText(item.text);
    }
    QImage image;
    if (!image.loadFromData(item.data)) {
        return false;
    }
    return WinClipboard::SetImage(image, imageScaleMaxEdge_);
}

// ── History store ─────────────────────────────────────────────────

void ClipboardLiteManager::SetMax(size_t maxItems) {
    history_.SetMaxItems(maxItems);
    MarkHistoryDirty();
    SaveMaxItemsSetting();
    UpdateTrayTip();
}

void ClipboardLiteManager::SetImageScaleMaxEdge(int maxEdge) {
    imageScaleMaxEdge_ = std::max(0, maxEdge);
    SaveImageScaleSetting();
    UpdateTrayTip();
}

void ClipboardLiteManager::SetCaptureEnabled(bool enabled) {
    if (captureEnabled_ == enabled) {
        return;
    }

    captureEnabled_ = enabled;
    retryCaptureTimer_->stop();
    retryCaptureCount_ = 0;
    suppressNextCapture_ = false;
    pendingClipboardSequence_ = 0;
    lastClipboardSequence_ = WinClipboard::ClipboardSequence();
    store_.SaveSettingInt("CaptureEnabled", captureEnabled_ ? 1 : 0);
}

void ClipboardLiteManager::LoadStoredHistory() {
    std::vector<ClipItem> stored = store_.Load(history_.MaxItems());
    if (!stored.empty()) {
        history_.ReplaceItems(std::move(stored));
    }
    historyDirty_ = false;
}

void ClipboardLiteManager::MarkHistoryDirty() {
    historyDirty_ = true;
}

void ClipboardLiteManager::SyncHistoryStore() {
    if (historyDirty_ && store_.Save(history_.Items(), history_.MaxItems())) {
        historyDirty_ = false;
    }
}

void ClipboardLiteManager::RefreshPopupIfVisible() {
    if (popup_ && popup_->IsPopupVisible()) {
        popup_->Refresh();
    }
}

void ClipboardLiteManager::ShowHintMessage(const QString& text) {
    if (popup_ && popup_->IsPopupVisible()) {
        for (const auto& hint : pendingHints_) {
            popup_->ShowHint(hint);
        }
        pendingHints_.clear();
        popup_->ShowHint(text);
    } else {
        pendingHints_.append(text);
    }
}

// ── Clipboard submenu (hosted by the application's tray) ──────────

void ClipboardLiteManager::UpdateTrayTip() {
    emit wakeHotkeyChanged(ActiveShowHotkeyLabel());
}

void ClipboardLiteManager::PopulateClipboardMenu(QMenu* menu) {
    if (!menu) {
        return;
    }
    menu->clear();

    QAction* captureAction = menu->addAction(QStringLiteral("Monitor clipboard"));
    MenuCheckMark::apply(captureAction, captureEnabled_);
    connect(captureAction, &QAction::triggered, this,
            [this]() { SetCaptureEnabled(!captureEnabled_); });
    menu->addSeparator();

    menu->addAction(QStringLiteral("Show history\t") + ActiveShowHotkeyLabel(),
                    this, [this]() { ShowHistory(true); });
    menu->addAction(QStringLiteral("Clear history"), this, [this]() {
        history_.Clear();
        MarkHistoryDirty();
        RefreshPopupIfVisible();
    });
    menu->addSeparator();

    QMenu* hotkeyMenu = menu->addMenu(QStringLiteral("Wake hotkey"));
    for (const HotkeyOption& option : ShowHotkeyOptions()) {
        AppendHotkeyItem(hotkeyMenu, option.commandId);
    }

    QMenu* pasteMenu = menu->addMenu(QStringLiteral("Paste hotkey"));
    for (const PasteHotkeyOption& option : PasteHotkeyOptions()) {
        AppendPasteHotkeyItem(pasteMenu, option.commandId);
    }

    QMenu* scaleMenu = menu->addMenu(QStringLiteral("Image paste size"));
    for (const ImageScaleOption& option : ImageScaleOptions()) {
        AppendImageScaleItem(scaleMenu, option.commandId);
    }

    QMenu* maxMenu = menu->addMenu(QStringLiteral("Max retained"));
    for (int value : kMaxHistoryValues) {
        AppendMaxItem(maxMenu, 0, static_cast<size_t>(value));
    }

    menu->addSeparator();

    menu->addAction(QStringLiteral("AI Fill Settings..."), this,
                    [this]() { ShowAiFillSettingsDialog(); });
}

void ClipboardLiteManager::AppendMaxItem(QMenu* menu, int, size_t value) {
    QAction* act = menu->addAction(QString::number(value) + QStringLiteral(" items"));
    MenuCheckMark::apply(act, history_.MaxItems() == value);
    connect(act, &QAction::triggered, this, [this, value]() { SetMax(value); });
}

void ClipboardLiteManager::AppendHotkeyItem(QMenu* menu, int id) {
    const HotkeyOption* option = FindShowHotkey(id);
    if (!option) {
        return;
    }
    QAction* act = menu->addAction(option->label);
    MenuCheckMark::apply(act, registeredShowHotkeyCommand_ == id);
    connect(act, &QAction::triggered, this, [this, id]() { SetShowHotkey(id); });
}

void ClipboardLiteManager::AppendPasteHotkeyItem(QMenu* menu, int id) {
    const PasteHotkeyOption* option = FindPasteHotkey(id);
    if (!option) {
        return;
    }
    QAction* act = menu->addAction(option->label);
    MenuCheckMark::apply(act, registeredPasteHotkeyCommand_ == id);
    connect(act, &QAction::triggered, this, [this, id]() { SetPasteHotkey(id); });
}

void ClipboardLiteManager::AppendImageScaleItem(QMenu* menu, int id) {
    const ImageScaleOption* option = FindImageScale(id);
    if (!option) {
        return;
    }
    QAction* act = menu->addAction(option->label);
    MenuCheckMark::apply(act, imageScaleMaxEdge_ == option->maxEdge);
    connect(act, &QAction::triggered, this, [this, id]() { SetImageScaleMaxEdge(FindImageScale(id)->maxEdge); });
}

QString ClipboardLiteManager::ActiveShowHotkeyLabel() const {
    const HotkeyOption* option = FindShowHotkey(registeredShowHotkeyCommand_);
    return option ? option->label : QString();
}

void ClipboardLiteManager::SavePopupPosition(const QPoint& position) {
    popupPosition_ = position;
    hasPopupPosition_ = true;
    store_.SaveSettingInt("PopupX", position.x());
    store_.SaveSettingInt("PopupY", position.y());
}

void ClipboardLiteManager::SavePopupSize(const QSize& size) {
    popupSize_ = size;
    const QPoint scalePoint = popup_ ? popup_->frameGeometry().center() : QCursor::pos();
    popupScaleFactor_ = qMax<qreal>(1.0, Util::getScreenScaleFactor(scalePoint));
    hasPopupSize_ = true;
    store_.SaveSettingInt("PopupWidth", size.width());
    store_.SaveSettingInt("PopupHeight", size.height());
    store_.SaveSettingInt("PopupScalePermille", qRound(popupScaleFactor_ * 1000.0));
}

QString ClipboardLiteManager::ActivePasteHotkeyLabel() const {
    return PasteHotkeyLabel(registeredPasteHotkeyCommand_, preferredPasteHotkeyCommand_);
}

QString ClipboardLiteManager::ActiveImageScaleLabel() const {
    return ImageScaleLabel(imageScaleMaxEdge_);
}

// ── Settings ──────────────────────────────────────────────────────

void ClipboardLiteManager::LoadSettings() {
    captureEnabled_ = store_.LoadSettingInt("CaptureEnabled", 1) != 0;

    int commandId = store_.LoadSettingInt("WakeHotkey", cl::id::HotkeyCtrlBacktick);
    if (FindShowHotkey(commandId)) {
        preferredShowHotkeyCommand_ = commandId;
    }

    commandId = store_.LoadSettingInt("PasteHotkey", cl::id::PasteHotkeyCtrlAltNum);
    if (FindPasteHotkey(commandId)) {
        preferredPasteHotkeyCommand_ = commandId;
    }

    const int maxItems = store_.LoadSettingInt("MaxItems", static_cast<int>(history_.MaxItems()));
    if (maxItems == 20 || maxItems == 50 || maxItems == 100 || maxItems == 200) {
        history_.SetMaxItems(static_cast<size_t>(maxItems));
    }

    const int imageScaleMaxEdge = store_.LoadSettingInt("ImageScaleMaxEdge", 0);
    if (FindImageScaleByMaxEdge(imageScaleMaxEdge)) {
        imageScaleMaxEdge_ = imageScaleMaxEdge;
    }

    aiFillSettings_.Load(store_);

    const int missing = std::numeric_limits<int>::min();
    const int popupX = store_.LoadSettingInt("PopupX", missing);
    const int popupY = store_.LoadSettingInt("PopupY", missing);
    if (popupX != missing && popupY != missing) {
        popupPosition_ = QPoint(popupX, popupY);
        hasPopupPosition_ = true;
    }
    const int popupWidth = store_.LoadSettingInt("PopupWidth", 0);
    const int popupHeight = store_.LoadSettingInt("PopupHeight", 0);
    popupScaleFactor_ = qMax(1, store_.LoadSettingInt("PopupScalePermille", 1000)) / 1000.0;
    if (popupWidth >= HistoryWindow::kMinimumWidth &&
        popupHeight >= HistoryWindow::kMinimumHeight) {
        popupSize_ = QSize(popupWidth, popupHeight);
        hasPopupSize_ = true;
    }
}

void ClipboardLiteManager::SaveSettings() {
    SaveShowHotkeySetting();
    SavePasteHotkeySetting();
    SaveMaxItemsSetting();
    SaveImageScaleSetting();
    SaveAiFillSettings();
}

void ClipboardLiteManager::SaveShowHotkeySetting() {
    store_.SaveSettingInt("WakeHotkey", preferredShowHotkeyCommand_);
}

void ClipboardLiteManager::SavePasteHotkeySetting() {
    store_.SaveSettingInt("PasteHotkey", preferredPasteHotkeyCommand_);
}

void ClipboardLiteManager::SaveMaxItemsSetting() {
    store_.SaveSettingInt("MaxItems", static_cast<int>(history_.MaxItems()));
}

void ClipboardLiteManager::SaveImageScaleSetting() {
    store_.SaveSettingInt("ImageScaleMaxEdge", imageScaleMaxEdge_);
}

void ClipboardLiteManager::SaveAiFillSettings() {
    aiFillSettings_.Save(store_);
}

void ClipboardLiteManager::SetShowHotkey(int commandId) {
    if (!FindShowHotkey(commandId)) {
        return;
    }
    preferredShowHotkeyCommand_ = commandId;
    if (RegisterShowHotkeyWithFallback()) {
        SaveShowHotkeySetting();
    } else {
        ShowHintMessage(QStringLiteral("No wake hotkey is available. Please close the conflicting app and try again."));
    }
    UpdateTrayTip();
}

void ClipboardLiteManager::SetPasteHotkey(int commandId) {
    if (!FindPasteHotkey(commandId)) {
        return;
    }
    preferredPasteHotkeyCommand_ = commandId;
    if (RegisterPasteHotkeysWithFallback()) {
        SavePasteHotkeySetting();
        UpdateTrayTip();
    } else {
        ShowHintMessage(QStringLiteral("No paste hotkey is available. Please close the conflicting app and try again."));
    }
}

bool ClipboardLiteManager::RegisterShowHotkeyWithFallback() {
    UnregisterHotKey(reinterpret_cast<HWND>(winId()), cl::id::HotkeyShowHistory);
    registeredShowHotkeyCommand_ = 0;

    if (TryRegisterShowHotkey(preferredShowHotkeyCommand_)) {
        return true;
    }
    for (const HotkeyOption& option : ShowHotkeyOptions()) {
        if (option.commandId != preferredShowHotkeyCommand_ &&
            TryRegisterShowHotkey(option.commandId)) {
            return true;
        }
    }
    return false;
}

bool ClipboardLiteManager::TryRegisterShowHotkey(int commandId) {
    const HotkeyOption* option = FindShowHotkey(commandId);
    if (!option) {
        return false;
    }
    if (!RegisterHotKey(reinterpret_cast<HWND>(winId()), cl::id::HotkeyShowHistory,
                        option->modifiers | MOD_NOREPEAT, option->key)) {
        return false;
    }
    registeredShowHotkeyCommand_ = commandId;
    return true;
}

bool ClipboardLiteManager::RegisterPasteHotkeysWithFallback() {
    UnregisterPasteHotkeys();
    registeredPasteHotkeyCommand_ = 0;

    if (TryRegisterPasteHotkeys(preferredPasteHotkeyCommand_)) {
        return true;
    }
    for (const PasteHotkeyOption& option : PasteHotkeyOptions()) {
        if (option.commandId != preferredPasteHotkeyCommand_ &&
            TryRegisterPasteHotkeys(option.commandId)) {
            return true;
        }
    }
    return false;
}

bool ClipboardLiteManager::TryRegisterPasteHotkeys(int commandId) {
    const PasteHotkeyOption* option = FindPasteHotkey(commandId);
    if (!option) {
        return false;
    }
    if (!RegisterHotKey(reinterpret_cast<HWND>(winId()), cl::id::HotkeyPasteFirst,
                        option->modifiers | MOD_NOREPEAT, '1')) {
        return false;
    }
    for (int i = 1; i < 9; ++i) {
        if (!RegisterHotKey(reinterpret_cast<HWND>(winId()), cl::id::HotkeyPasteFirst + i,
                            option->modifiers | MOD_NOREPEAT, static_cast<UINT>('1' + i))) {
            UnregisterPasteHotkeys();
            return false;
        }
    }
    registeredPasteHotkeyCommand_ = commandId;
    return true;
}

void ClipboardLiteManager::UnregisterPasteHotkeys() {
    for (int id = cl::id::HotkeyPasteFirst; id <= cl::id::HotkeyPasteLast; ++id) {
        UnregisterHotKey(reinterpret_cast<HWND>(winId()), id);
    }
}

UINT ClipboardLiteManager::RegisteredPasteHotkeyModifiers() const {
    const PasteHotkeyOption* option = FindPasteHotkey(registeredPasteHotkeyCommand_);
    return option ? option->modifiers : 0;
}

// ── AI Fill ──────────────────────────────────────────────────────

void ClipboardLiteManager::StartAiFill(size_t index) {
    const auto& items = history_.Items();
    if (index >= items.size()) {
        return;
    }
    const ClipItem& item = items[index];
    if (item.kind != ClipKind::Text) {
        return;
    }

    if (!aiFillSettings_.IsConfigured()) {
        ShowHintMessage(QStringLiteral("AI Fill is not configured. Open AI Fill Settings from the tray menu."));
        return;
    }
    if (aiFillRunner_.busy()) {
        ShowHintMessage(QStringLiteral("AI Fill is in progress. Please wait for the current task to complete."));
        return;
    }

    aiFillController_ = std::make_unique<AiFillController>(aiFillSettings_.ToLlmConfig());
    if (!aiFillController_->Prepare(targetWindow_, item.text)) {
        ShowHintMessage(aiFillController_->LastError());
        return;
    }

    aiFillResults_ = std::make_shared<std::vector<FillResult>>();
    aiFillError_ = std::make_shared<QString>();

    auto results = aiFillResults_;
    auto error = aiFillError_;
    auto* controller = aiFillController_.get();

    const bool started = aiFillRunner_.start(
        [controller, results, error]() {
            *results = controller->RequestLlmFill();
            *error = controller->LastError();
        },
        [this]() { OnAiFillWorkerDone(); });

    if (!started) {
        ShowHintMessage(QStringLiteral("AI Fill failed to start. The previous task may still be running."));
    }
}

void ClipboardLiteManager::OnAiFillWorkerDone() {
    if (!aiFillError_->isEmpty()) {
        ShowHintMessage(*aiFillError_);
        aiFillResults_.reset();
        aiFillError_.reset();
        return;
    }

    if (aiFillResults_->empty()) {
        ShowHintMessage(QStringLiteral("AI Fill completed. No controls were filled."));
        aiFillResults_.reset();
        aiFillError_.reset();
        return;
    }

    const bool applied = aiFillController_->ApplyResults(*aiFillResults_);
    Q_UNUSED(applied);
    const int filledCount = aiFillController_->LastAppliedCount();
    const int failedCount = aiFillController_->LastFailedCount();

    QString message;
    if (filledCount > 0) {
        message = QStringLiteral("AI Fill filled %1 control%2.")
                      .arg(filledCount)
                      .arg(filledCount == 1 ? QString() : QStringLiteral("s"));
        if (failedCount > 0) {
            message += QStringLiteral(" %1 failed.").arg(failedCount);
        }
        ShowHintMessage(message);
    } else {
        message = QStringLiteral("AI Fill completed. No controls were filled.");
        if (!applied && !aiFillController_->LastError().isEmpty()) {
            message += QStringLiteral(" ") + aiFillController_->LastError();
        }
        ShowHintMessage(message);
    }

    aiFillResults_.reset();
    aiFillError_.reset();
}

void ClipboardLiteManager::ShowAiFillSettingsDialog() {
    AiFillSettingsDialog dlg(aiFillSettings_, popup_ ? static_cast<QWidget*>(popup_) : this);
    if (dlg.exec() == QDialog::Accepted) {
        aiFillSettings_.Save(store_);
        UpdateTrayTip();
    }
}

// ── Popup ────────────────────────────────────────────────────────

void ClipboardLiteManager::ShowHistory(bool activate) {
    EnsurePopup();
    popup_->ShowPopup(activate);
    if (!pendingHints_.isEmpty()) {
        for (const auto& hint : pendingHints_) {
            popup_->ShowHint(hint);
        }
        pendingHints_.clear();
    }
}

void ClipboardLiteManager::EnsurePopup() {
    auto paste = [this](size_t index) { BeginPaste(index, true); };
    auto remove = [this](size_t index) {
        if (history_.RemoveAt(index)) {
            MarkHistoryDirty();
            RefreshPopupIfVisible();
        }
    };
    auto close = [this]() {};
    auto aiFill = [this](size_t index) { StartAiFill(index); };
    auto moved = [this](const QPoint& position) { SavePopupPosition(position); };
    auto resized = [this](const QSize& size) { SavePopupSize(size); };
    const bool configured = aiFillSettings_.IsConfigured();
    if (popup_) {
        popup_->Configure(&history_, paste, remove, close, aiFill, moved, resized, configured);
        popup_->SetSavedPosition(popupPosition_, hasPopupPosition_);
        popup_->SetSavedSize(popupSize_, hasPopupSize_, popupScaleFactor_);
        return;
    }
    const QPoint scalePoint = hasPopupPosition_ ? popupPosition_ : QCursor::pos();
    popup_ = new HistoryWindow(Util::getScreenScaleFactor(scalePoint));
    popup_->Configure(&history_, paste, remove, close, aiFill, moved, resized, configured);
    popup_->SetSavedPosition(popupPosition_, hasPopupPosition_);
    popup_->SetSavedSize(popupSize_, hasPopupSize_, popupScaleFactor_);
}

// ── Timer handlers ───────────────────────────────────────────────

void ClipboardLiteManager::onPasteTimer() {
    if (!pendingPasteReady_) {
        return;
    }
    if (pendingPasteTriggerKey_ != 0 &&
        (GetAsyncKeyState(static_cast<int>(pendingPasteTriggerKey_)) & 0x8000)) {
        pasteTimer_->start(15);
        return;
    }
    pendingPasteReady_ = false;
    const bool keepPopupVisible = pendingKeepPopupVisible_;
    pendingKeepPopupVisible_ = false;
    RestoreTargetFocus();
    WinClipboard::SendPasteForHotkey(pendingPasteReleaseModifiers_);
    pendingPasteReleaseModifiers_ = 0;
    pendingPasteTriggerKey_ = 0;
    if (!keepPopupVisible && popup_ && popup_->IsPopupVisible()) {
        popup_->HidePopup();
    }
}

void ClipboardLiteManager::onRetryCaptureTimer() {
    if (!captureEnabled_) {
        retryCaptureTimer_->stop();
        retryCaptureCount_ = 0;
        return;
    }

    const CaptureResult result = CaptureClipboardContent();
    const bool retryExpired = (result == CaptureResult::Failed) && (++retryCaptureCount_ >= 10);
    if (result != CaptureResult::Failed || retryExpired) {
        retryCaptureTimer_->stop();
        retryCaptureCount_ = 0;
        if (result == CaptureResult::Captured || retryExpired) {
            RefreshPopupIfVisible();
        }
    }
}

void ClipboardLiteManager::onPollTimer() {
    const DWORD sequence = WinClipboard::ClipboardSequence();
    if (!captureEnabled_) {
        lastClipboardSequence_ = sequence;
        return;
    }

    if (sequence != 0 && sequence != lastClipboardSequence_) {
        lastClipboardSequence_ = sequence;
        retryCaptureCount_ = 0;
        const CaptureResult result = CaptureClipboardContent();
        if (result == CaptureResult::Failed) {
            retryCaptureTimer_->start(80);
        } else if (result == CaptureResult::Captured) {
            RefreshPopupIfVisible();
        }
    }
}

void ClipboardLiteManager::onTrackTimer() {
    CaptureTargetFromForeground();
}

void ClipboardLiteManager::onSyncTimer() {
    SyncHistoryStore();
}
