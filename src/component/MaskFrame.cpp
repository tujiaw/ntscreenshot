#include "MaskFrame.h"
#include <QPaintEvent>
#include <QPainter>
#include <QHBoxLayout>
#include <QStyleOption>
#include <QStyle>
#include <qapplication.h>
#include <Windows.h>

MaskFrame::MaskFrame(QFrame *parent)
    : QFrame(parent)
{
    setWindowOpacity(0.3);
    setMouseTracking(true);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setAttribute(Qt::WA_DeleteOnClose, true);
    SetWindowPos((HWND)winId(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
    long initStyle = ::GetWindowLong((HWND)winId(), GWL_EXSTYLE);
    ::SetWindowLong((HWND)winId(), GWL_EXSTYLE, initStyle | WS_EX_TRANSPARENT | WS_EX_LAYERED);
    this->setAutoFillBackground(true);
}

void MaskFrame::paintEvent(QPaintEvent *e)
{
    QFrame::paintEvent(e);
}
