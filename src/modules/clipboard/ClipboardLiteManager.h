#pragma once

#include "AiFillController.h"
#include "AiFillSettings.h"
#include "core/foundation/AsyncRunner.h"
#include "core/modules/IToolModule.h"
#include "ClipboardHistory.h"
#include "ClipboardStore.h"
#include "Constants.h"
#include "HistoryWindow.h"

#include <windows.h>

#include <QMenu>
#include <QObject>
#include <QWidget>

#include <memory>
#include <vector>

// Self-contained clipboard history manager, ported from wtl_clipboard to Qt.
// Owns global hotkeys, clipboard capture, paste simulation, persistence and
// AI Fill orchestration. It does NOT own a tray icon: the host application's
// tray menu hosts the clipboard as a submenu (see PopulateClipboardMenu).
// Initialize once from main().
class ClipboardLiteManager : public QWidget, public IToolModule {
    Q_OBJECT

public:
    QString id() const override { return QStringLiteral("clipboard"); }
    void initialize() override;
    void shutdown() override;

    // Populate a clipboard submenu hosted by the application's tray menu.
    void PopulateClipboardMenu(QMenu* menu);
    QString ActiveShowHotkeyLabel() const;

signals:
    void wakeHotkeyChanged(const QString& hotkey);

protected:
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

private slots:
    void onPasteTimer();
    void onRetryCaptureTimer();
    void onPollTimer();
    void onTrackTimer();
    void onSyncTimer();

private:
    enum class CaptureResult { Failed, Captured, IgnoredOwnChange };

    // Clipboard capture
    CaptureResult CaptureClipboardContent();
    CaptureResult CaptureClipboardText();
    CaptureResult CaptureClipboardImage();
    bool ConsumeOwnClipboardChange(const ClipItem& item);
    void CaptureTargetFromForeground();

    // Paste
    void BeginPaste(size_t index, bool keepPopupReady, UINT releaseModifiers = 0, UINT triggerKey = 0);
    void RestoreTargetFocus();
    bool SetClipboardItem(const ClipItem& item);

    // History store
    void SetMax(size_t maxItems);
    void SetImageScaleMaxEdge(int maxEdge);
    void SetCaptureEnabled(bool enabled);
    void LoadStoredHistory();
    void MarkHistoryDirty();
    void SyncHistoryStore();
    void RefreshPopupIfVisible();
    void ShowHintMessage(const QString& text);
    void SavePopupPosition(const QPoint& position);
    void SavePopupSize(const QSize& size);

    // Clipboard submenu (hosted by the application's tray menu)
    void UpdateTrayTip();
    void AppendMaxItem(QMenu* menu, int id, size_t value);
    void AppendHotkeyItem(QMenu* menu, int id);
    void AppendPasteHotkeyItem(QMenu* menu, int id);
    void AppendImageScaleItem(QMenu* menu, int id);
    QString ActivePasteHotkeyLabel() const;
    QString ActiveImageScaleLabel() const;

    // Settings
    void LoadSettings();
    void SaveSettings();
    void SaveShowHotkeySetting();
    void SavePasteHotkeySetting();
    void SaveMaxItemsSetting();
    void SaveImageScaleSetting();
    void SaveAiFillSettings();
    void SetShowHotkey(int commandId);
    void SetPasteHotkey(int commandId);
    bool RegisterShowHotkeyWithFallback();
    bool TryRegisterShowHotkey(int commandId);
    bool RegisterPasteHotkeysWithFallback();
    bool TryRegisterPasteHotkeys(int commandId);
    void UnregisterPasteHotkeys();
    UINT RegisteredPasteHotkeyModifiers() const;

    // AI Fill
    void StartAiFill(size_t index);
    void OnAiFillWorkerDone();
    void ShowAiFillSettingsDialog();

    // Popup
    void ShowHistory(bool activate = false);
    void EnsurePopup();

    // Shared state for AI Fill cross-thread results
    std::shared_ptr<std::vector<FillResult>> aiFillResults_;
    std::shared_ptr<QString> aiFillError_;

    ClipboardHistory history_;
    ClipboardStore store_;
    HistoryWindow* popup_ = nullptr;
    AiFillSettings aiFillSettings_;
    std::unique_ptr<AiFillController> aiFillController_;
    AsyncRunner aiFillRunner_;

    QTimer* pasteTimer_ = nullptr;
    QTimer* retryCaptureTimer_ = nullptr;
    QTimer* pollTimer_ = nullptr;
    QTimer* trackTimer_ = nullptr;
    QTimer* syncTimer_ = nullptr;

    HWND targetWindow_ = nullptr;
    HWND targetFocusWindow_ = nullptr;

    int retryCaptureCount_ = 0;
    bool suppressNextCapture_ = false;
    bool pendingPasteReady_ = false;
    bool pendingKeepPopupVisible_ = false;
    UINT pendingPasteReleaseModifiers_ = 0;
    UINT pendingPasteTriggerKey_ = 0;
    ClipItem pendingClipboardItem_;
    DWORD pendingClipboardSequence_ = 0;
    DWORD lastClipboardSequence_ = 0;

    int preferredShowHotkeyCommand_ = cl::id::HotkeyCtrlBacktick;
    int registeredShowHotkeyCommand_ = 0;
    int preferredPasteHotkeyCommand_ = cl::id::PasteHotkeyCtrlAltNum;
    int registeredPasteHotkeyCommand_ = 0;
    int imageScaleMaxEdge_ = 0;

    bool historyDirty_ = false;
    bool captureEnabled_ = true;
    bool hasPopupPosition_ = false;
    QPoint popupPosition_;
    bool hasPopupSize_ = false;
    QSize popupSize_;
    qreal popupScaleFactor_ = 1.0;
    bool initialized_ = false;
    QStringList pendingHints_;
};
