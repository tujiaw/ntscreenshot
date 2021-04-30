#pragma once

#include <QFrame>
#include <QWidget>

class MaskFrame : public QFrame
{
    Q_OBJECT

public:
    MaskFrame(QFrame *parent = 0);

protected:
    void paintEvent(QPaintEvent *p);

private:
    QWidget *widget_;
};
