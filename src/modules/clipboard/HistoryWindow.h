#pragma once

#include "ClipboardHistory.h"

#include <QAbstractItemDelegate>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPoint>
#include <QSizeGrip>
#include <QTimer>
#include <QToolButton>
#include <QButtonGroup>
#include <QWidget>
#include <QStackedWidget>

#include <functional>

class ClipboardPreviewWindow;

class HistoryWindow : public QWidget {
    Q_OBJECT

public:
    using PasteCallback = std::function<void(size_t)>;
    using DeleteCallback = std::function<void(size_t)>;
    using CloseCallback = std::function<void()>;
    using AiFillCallback = std::function<void(size_t)>;
    using MoveCallback = std::function<void(const QPoint&)>;
    using ResizeCallback = std::function<void(const QSize&)>;
    using EditCallback = std::function<void(size_t, const QString&)>;

    explicit HistoryWindow(qreal scaleFactor, QWidget* parent = nullptr);

    void Configure(const ClipboardHistory* history,
                   PasteCallback paste,
                   DeleteCallback remove,
                   CloseCallback close,
                   AiFillCallback aiFill,
                   MoveCallback moved,
                   ResizeCallback resized,
                   bool aiConfigured,
                   DeleteCallback toggleFavorite = {},
                   EditCallback edit = {});
    void SetSavedPosition(const QPoint& position, bool available);
    void SetBackgroundTransparency(int percent);
    void SetSavedSize(const QSize& size, bool available, qreal savedScaleFactor = 1.0);

    void ShowPopup(bool activate = false);
    void HidePopup();
    void Refresh();
    void RefreshItemText(size_t index);
    void ShowHint(const QString& text, int timeoutMs = 2400);
    bool IsPopupVisible() const;
    size_t HistoryIndexForRow(int row) const;
    void SetSyncStatus(const QString& status);
    void SetSyncPage(QWidget* page);
    void ShowSyncPage();

signals:
    void qrCodeRequested(const QString& text);

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onSearchChanged();
    void onItemActivated(QListWidgetItem* item);
    void onRelativeTimeTick();
    void onCloseClicked();

private:
    void PasteSelected();
    void PasteRow(int row);
    void CopySelected();
    void DeleteSelected();
    void AiFillSelected();
    void ToggleFavoriteSelected();
    void TogglePreview();
    void ShowPreview();
    void HidePreview();
    void UpdatePreview();
    void PreviewItem(const ClipItem& item);
    void EnsurePreview();
    void CommitPendingEdit();
    void ApplyPreviewTheme();
    void BuildList();
    void ShowContextMenu(const QPoint& globalPos);
    void ApplyTheme();
    void ShowToast(const QString& text, int timeoutMs);
    int scaled(int value) const;

    const ClipboardHistory* history_ = nullptr;
    PasteCallback paste_;
    DeleteCallback remove_;
    DeleteCallback toggleFavorite_;
    CloseCallback close_;
    AiFillCallback aiFill_;
    MoveCallback moved_;
    ResizeCallback resized_;
    EditCallback edit_;
    bool aiConfigured_ = false;
    bool hasSavedPosition_ = false;
    QPoint savedPosition_;

    QWidget* header_ = nullptr;
    ClipboardPreviewWindow* preview_ = nullptr;
    bool previewVisible_ = false;
    size_t previewIndex_ = static_cast<size_t>(-1);
    QToolButton* closeButton_ = nullptr;
    QToolButton* clearSearchButton_ = nullptr;
    QLineEdit* searchEdit_ = nullptr;
    QButtonGroup* viewGroup_ = nullptr;
    QWidget* viewSwitcher_ = nullptr;
    QToolButton* clipboardViewButton_ = nullptr;
    QToolButton* favoritesViewButton_ = nullptr;
    QStackedWidget* pages_ = nullptr;
    QStackedWidget* bodyStack_ = nullptr;
    QWidget* syncPage_ = nullptr;
    QToolButton* syncSettingsButton_ = nullptr;
    QWidget* body_ = nullptr;
    QListWidget* list_ = nullptr;
    QAbstractItemDelegate* delegate_ = nullptr;
    QWidget* emptyState_ = nullptr;
    QLabel* emptyTitle_ = nullptr;
    QLabel* emptyDescription_ = nullptr;
    QLabel* toastLabel_ = nullptr;
    QSizeGrip* resizeGrip_ = nullptr;

    QTimer* timeTimer_ = nullptr;
    QTimer* toastTimer_ = nullptr;
    QTimer* resizeSaveTimer_ = nullptr;

    bool dragging_ = false;
    QPoint dragStartPos_;
    QString lastQuery_;
    qreal scaleFactor_ = 1.0;
    int backgroundAlpha_ = 255;

public:
    static constexpr int kPopupWidth = 340;
    static constexpr int kPopupHeight = 360;
    static constexpr int kMinimumWidth = 280;
    static constexpr int kMinimumHeight = 240;
    static constexpr int kRadius = 8;
};
