#pragma once

#include <memory>
#include <QPoint>
#include <QRect>
#include <QWidget>

class TextSelectionToolbar;

class TextSelectionWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TextSelectionWidget(QWidget *parent = nullptr);

signals:
    void sigClose();
    void sigActionTriggered(const QString &actionId, const QRect &selectionRect);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onToolbarActionTriggered(const QString &actionId, const QString &inputText);

private:
    enum class SelectionState {
        Idle,
        Selecting,
        Selected
    };

    QRect normalizedSelection(const QPoint &endPoint) const;
    bool hasValidSelection() const;
    void resetSelection();
    void showToolbar();

    SelectionState state_;
    QPoint startPoint_;
    QRect selectionRect_;
    std::unique_ptr<TextSelectionToolbar> toolbar_;

    static constexpr int MIN_SELECTION_EDGE = 12;
};
