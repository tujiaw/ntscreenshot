#pragma once

#include <QEvent>
#include <QWidget>
#include "core/settings/SettingModel.h"

class QHBoxLayout;
class QLineEdit;
class QPropertyAnimation;

class TextSelectionToolbar : public QWidget
{
    Q_OBJECT

public:
    explicit TextSelectionToolbar(QWidget *parent = nullptr);
    void setActions(const QList<TextSelectionActionConfig> &actions);
    void showForSelection(const QRect &selectionRect, const QRect &boundsRect);
    void showNearGlobalPoint(const QPoint &globalPoint);
    bool containsGlobalPoint(const QPoint &globalPoint) const;

signals:
    void sigActionTriggered(const QString &actionId, const QString &inputText);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void rebuildActionButtons();
    void addActionButton(QHBoxLayout *layout, const QString &actionId, const QString &text);
    void addChatInput(QHBoxLayout *layout);
    void refreshCompactSize();
    void resetCompactPresentation();
    void setChatInputExpanded(bool expanded);
    void refreshChatInputStyle(bool expanded);

    QHBoxLayout *layout_;
    QWidget *dragHandle_ = nullptr;
    QWidget *actionContainer_ = nullptr;
    QHBoxLayout *actionLayout_ = nullptr;
    QLineEdit *chatInput_ = nullptr;
    QPropertyAnimation *chatInputAnimation_ = nullptr;
    int compactWidth_ = 0;
    int compactHeight_ = 0;
    bool chatInputExpanded_ = false;
    bool pendingShowActionsAfterCollapse_ = false;
    bool dragging_ = false;
    QPoint dragOffset_;
    QList<TextSelectionActionConfig> actions_;
};
