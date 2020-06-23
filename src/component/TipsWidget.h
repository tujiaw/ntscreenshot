#pragma once

#include <QLabel>

class TipsWidget : public QLabel
{
    Q_OBJECT

public:
    TipsWidget(QWidget *parent);
    ~TipsWidget();
    void autoClose(int seconds);
    void setYOffset(int offset);
    void setClickedCopy(bool enable);

    static void popup(QWidget *parent, const QString &text, int seconds, int yOffset = 0, bool clickedCopy = false);

protected:
    void showEvent(QShowEvent *event);
    void enterEvent(QEvent *event);
    void leaveEvent(QEvent *event);
    void mousePressEvent(QMouseEvent *ev);

private slots:
    void onTimeout();

private:
    QWidget *parent_;
    QTimer *timer_;
    int yOffset_;
    bool isClickedCopy_;
};
