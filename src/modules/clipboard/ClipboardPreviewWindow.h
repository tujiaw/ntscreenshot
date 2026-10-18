#pragma once

#include "ClipItem.h"
#include <QWidget>

class QStackedWidget;
class QPlainTextEdit;
class QGraphicsView;
class QLabel;

class ClipboardPreviewWindow final : public QWidget {
    Q_OBJECT
public:
    explicit ClipboardPreviewWindow(QWidget* owner);
    void ShowItem(const ClipItem& item);
signals:
    void navigateRequested(int delta);
protected:
    bool eventFilter(QObject* object, QEvent* event) override;
private:
    void CloseIfFocusOutside();
    QWidget* owner_;
    QStackedWidget* pages_;
    QPlainTextEdit* text_;
    QGraphicsView* image_;
    QLabel* error_;
    ClipItem current_;
    bool hasContent_ = false;
};
