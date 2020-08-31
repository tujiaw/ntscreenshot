#include "Amplifier.h"
#include <QPixmap>
#include <QPainter>
#include <QKeyEvent>
#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include "common/Constants.h"

const QSize IMAGE_SIZE(30, 22);
const int MULTIPLE = 4;
AmplifierWidget::AmplifierWidget(const std::shared_ptr<QPixmap> &originPainting, QWidget *parent) 
    : QWidget(parent), originPainting_(originPainting)
{
    setWindowFlags(Qt::FramelessWindowHint|Qt::WindowSystemMenuHint);
    setMouseTracking(true);
    imageHeight_ = IMAGE_SIZE.height() * MULTIPLE;
    setFixedSize(IMAGE_SIZE.width() * MULTIPLE, imageHeight_ + 34);
    hide();
}

QString AmplifierWidget::getCursorPointColor() const
{
    QString colorInfo;
    if (isRgbColor_) {
        colorInfo = QString("RGB(%1, %2, %3)").arg(cursorPointColor_.red()).arg(cursorPointColor_.green()).arg(cursorPointColor_.blue());
    } else {
        colorInfo = cursorPointColor_.name().toUpper();
    }
    return colorInfo;
}

void AmplifierWidget::onSizeChanged(int w, int h) 
{
    QPoint p(QCursor::pos());
    onPositionChanged(p.x(), p.y());
}

void AmplifierWidget::onPositionChanged(int x, int y) 
{
    cursorPoint_ = QPoint(x, y);
    int dest_x = x + 4;
    int dest_y = y + 26;

    /// 超出屏幕检测
    const QSize& parent_size = parentWidget()->size();
    if (dest_y + height() > parent_size.height()) {
        dest_y = y - 26 - height();
    }
    if (dest_x + width() > parent_size.width()) {
        dest_x = x - 4 - width();
    }

    move(dest_x, dest_y);
}

//// 绘制鼠标拖拽时选区矩形的右下顶点的放大图;
void AmplifierWidget::paintEvent(QPaintEvent *) 
{
    QPainter painter(this);
    /// 绘制背景
    painter.fillRect(rect(), QColor(0, 0, 0, 200));
    QPixmap endPointImage;
    /// 绘制放大图;
    const QSize& parent_size = parentWidget()->size();
    
    int mx = IMAGE_SIZE.width() / 2;
    int my = IMAGE_SIZE.height() / 2;
    if ((cursorPoint_.x() + mx < parent_size.width() && cursorPoint_.x() - mx > 0)
        && (cursorPoint_.y() + my < parent_size.height() && cursorPoint_.y() - my > 0)) {
        endPointImage = originPainting_->copy(QRect(cursorPoint_.x() - mx, cursorPoint_.y() - my, IMAGE_SIZE.width(), IMAGE_SIZE.height()))
            .scaled(width() - 2, imageHeight_ - 2);
    } else {
        // 光标在屏幕边缘
        endPointImage = originPainting_->copy(QRect(cursorPoint_.x(), cursorPoint_.y(), IMAGE_SIZE.width(), IMAGE_SIZE.height()))
        .scaled(width() - 2, imageHeight_ - 2);
    }
    painter.drawPixmap(0, 0, endPointImage);

    /// 当前鼠标像素值的RGB信息
    QImage image = originPainting_->toImage();
    cursorPointColor_ = image.pixel(cursorPoint_);

    /// 绘制十字
    const int PEN_WIDTH = MULTIPLE;
    painter.setPen(QPen(QColor(0, 180, 255, 180), PEN_WIDTH));
    /// 竖线;
    painter.drawLine(QPoint((width() >> 1) + 1, 0), QPoint((width() >> 1) + 1, imageHeight_ - MULTIPLE));
    /// 横线;
    painter.drawLine(QPoint(0, (imageHeight_ >> 1) + 1), QPoint(height(), (imageHeight_ >> 1) + 1));

    /// 绘制中间颜色块
    painter.fillRect((width() >> 1) - 2, (imageHeight_ >> 1) - 2, 6, 6, cursorPointColor_);
    /// 绘制中间颜色块边框
    painter.setPen(QPen(Qt::black, 1));
    painter.drawRect((width() >> 1) - 3, (imageHeight_ >> 1) - 3, 7, 7);

    /// 绘制大图内边框
    painter.setPen(QPen(Qt::white, 2));
    painter.drawRect(2,2,width()-4, imageHeight_-4);

    /// 绘制外边框
    painter.setPen(QPen(Qt::black, 1));
    painter.drawRect(0,0,width()-1,height()-1);

    /// 当前选中矩形的宽高信息;
    QString posInfo = QStringLiteral("%1 x %2").arg(QCursor::pos().x()).arg(QCursor::pos().y());
    
    const int SPACING = 4;
    // 绘制坐标轴相关数据
    painter.setPen(Qt::white);
    int x = SPACING;
    int y = imageHeight_ + painter.font().pointSize() + SPACING;
    painter.drawText(QPoint(x, y), posInfo);
    y += painter.font().pointSize() + SPACING;
    painter.drawText(QPoint(x, y), getCursorPointColor());
}
