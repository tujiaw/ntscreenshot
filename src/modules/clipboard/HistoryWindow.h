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
#include <QWidget>

#include <functional>

class HistoryWindow : public QWidget {
    Q_OBJECT

public:
    using PasteCallback = std::function<void(size_t)>;
    using DeleteCallback = std::function<void(size_t)>;
    using CloseCallback = std::function<void()>;
    using AiFillCallback = std::function<void(size_t)>;
    using MoveCallback = std::function<void(const QPoint&)>;
    using ResizeCallback = std::function<void(const QSize&)>;

    explicit HistoryWindow(qreal scaleFactor, QWidget* parent = nullptr);

    void Configure(const ClipboardHistory* history,
                   PasteCallback paste,
                   DeleteCallback remove,
                   CloseCallback close,
                   AiFillCallback aiFill,
                   MoveCallback moved,
                   ResizeCallback resized,
                   bool aiConfigured);
    void SetSavedPosition(const QPoint& position, bool available);
    void SetSavedSize(const QSize& size, bool available, qreal savedScaleFactor = 1.0);

    void ShowPopup(bool activate = false);
    void HidePopup();
    void Refresh();
    void ShowHint(const QString& text, int timeoutMs = 2400);
    bool IsPopupVisible() const;

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
    size_t HistoryIndexForRow(int row) const;
    void PasteSelected();
    void PasteRow(int row);
    void CopySelected();
    void DeleteSelected();
    void AiFillSelected();
    void ScanSelectedCode();
    void FindSimilarSelected();
    void ExtractColorsSelected();
    void BuildList();
    void ShowContextMenu(const QPoint& globalPos);
    QImage DecodeHistoryImage(size_t index) const;
    void ApplyTheme();
    void ShowToast(const QString& text, int timeoutMs);
    int scaled(int value) const;

    const ClipboardHistory* history_ = nullptr;
    PasteCallback paste_;
    DeleteCallback remove_;
    CloseCallback close_;
    AiFillCallback aiFill_;
    MoveCallback moved_;
    ResizeCallback resized_;
    bool aiConfigured_ = false;
    bool hasSavedPosition_ = false;
    QPoint savedPosition_;

    QWidget* header_ = nullptr;
    QLabel* dragHandle_ = nullptr;
    QToolButton* closeButton_ = nullptr;
    QLineEdit* searchEdit_ = nullptr;
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

public:
    static constexpr int kPopupWidth = 340;
    static constexpr int kPopupHeight = 360;
    static constexpr int kMinimumWidth = 280;
    static constexpr int kMinimumHeight = 240;
    static constexpr int kRadius = 8;
};
