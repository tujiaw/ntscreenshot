#pragma once

#include "ClipItem.h"
#include <QWidget>

class QStackedWidget;
class QPlainTextEdit;
class QGraphicsView;
class QLabel;
class QToolButton;

// Embedded preview pane shown inside the popup's body area. The text is
// read-only until the user toggles edit mode; a floating toolbar offers a
// back button and an edit/save button.
class ClipboardPreviewWindow final : public QWidget {
    Q_OBJECT
public:
    explicit ClipboardPreviewWindow(QWidget* parent = nullptr);
    void ShowItem(const ClipItem& item);
    // Flushes an in-progress text edit (emits editCommitted) and returns true
    // if anything was committed.
    bool CommitEdit();
    // Focuses the image view so arrow keys navigate between items.
    void FocusImage();
signals:
    void navigateRequested(int delta);
    void dismissRequested();
    void editCommitted(const QString& text);
protected:
    bool eventFilter(QObject* object, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
private:
    void SetEditing(bool editing);
    void onEditClicked();
    void onBackClicked();

    QStackedWidget* pages_;
    QPlainTextEdit* text_;
    QGraphicsView* image_;
    QLabel* error_;
    QToolButton* backButton_ = nullptr;
    QToolButton* editButton_ = nullptr;
    ClipItem current_;
    bool hasContent_ = false;
    bool loading_ = false;
    bool textDirty_ = false;
    bool editing_ = false;
};
