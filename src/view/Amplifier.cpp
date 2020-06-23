#include "Amplifier.h"
#include <QPixmap>
#include <QPainter>
#include <QKeyEvent>
#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include "common/Constants.h"

AmplifierWidget::AmplifierWidget(const std::shared_ptr<QPixmap> &originPainting, QWidget *parent) 
    : QWidget(parent), originPainting_(originPainting)
{
    setWindowFlags(Qt::FramelessWindowHint|Qt::WindowSystemMenuHint);
    setMouseTracking(true);
    imageHeight_ = Style::OEAMPLIFIER_IMAGE_HEIGHT;
    setFixedSize(Style::OEAMPLIFIER_SIZE);
    hide();
}

QColor AmplifierWidget::getCursorPointColor() const
{
    return cursorPointColor_;
}

void AmplifierWidget::onSizeChange(int w, int h) {
    QPoint p(QCursor::pos());
    onPostionChange(p.x(), p.y());
}

void AmplifierWidget::onPostionChange(int x, int y) {
    cursorPoint_ = QPoint(x, y);
    int dest_x = x + 4;
    int dest_y = y + 26;

    /// ������Ļ���
    const QSize& parent_size = parentWidget()->size();
    if (dest_y + height() > parent_size.height()) {
        dest_y = y - 26 - height();
    }
    if (dest_x + width() > parent_size.width()) {
        dest_x = x - 4 - width();
    }

    move(dest_x, dest_y);
}

/// ���������קʱѡ�����ε����¶���ķŴ�ͼ;
void AmplifierWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    /// ���Ʊ���
    painter.fillRect(rect(), QColor(0, 0, 0, 160));
    QPixmap endPointImage;
    /// ���ƷŴ�ͼ;
    const QSize& parent_size = parentWidget()->size();
    /**
     * @bug   : ����Ļ��Ե���ƷŴ�ͼʱ�����ͼƬ����
     *          ������ʱ���˱�Ե��⣬������Ļ��Ե�򲻽��зŴ�ͼ�Ļ��ƣ���QQ��ͼ�Ĳ�ȡ��ʽ��һ�µġ�
     *
     * @marker: ��ɫ��������ʶ�𣬵��Ǿֲ��Ŵ��Ч����ʱ����
     *
     * @note  : ������������Է��ֱ�Եʱ�������ܷŴ�ĵط�������棬������ɫ���Ա���ͼƬ����Ԥ�ڵ��������⡣
     */
    if ((cursorPoint_.x() + 15 < parent_size.width() && cursorPoint_.x() - 15 > 0)
      && (cursorPoint_.y() + 11 < parent_size.height() && cursorPoint_.y() - 11 > 0)) {
        endPointImage = originPainting_->copy(QRect(cursorPoint_.x() - 15, cursorPoint_.y() - 11, 30, 22)).scaled(width(), imageHeight_);
    } else {
        endPointImage = originPainting_->copy(QRect(cursorPoint_.x() - 15, cursorPoint_.y() - 11, 30, 22));
    }
    painter.drawPixmap(0, 0, endPointImage);

    /// ��ǰ�������ֵ��RGB��Ϣ
    QImage image = originPainting_->toImage();
    cursorPointColor_ = image.pixel(cursorPoint_);

    /// ����ʮ��
    const int PEN_WIDTH = 4;
    painter.setPen(QPen(QColor(0, 180, 255, 180), PEN_WIDTH));
    // ����;
    painter.drawLine(QPoint((width() >> 1) + 1, 0), QPoint((width() >> 1) + 1, imageHeight_ - 4));
    // ����;
    painter.drawLine(QPoint(0, (imageHeight_ >> 1) + 1), QPoint(height(), (imageHeight_ >> 1) + 1));

    // �����м���ɫ��
    painter.fillRect((width() >> 1) - 2, (imageHeight_ >> 1) - 2, 6, 6, cursorPointColor_);
    // �����м���ɫ��߿�
    painter.setPen(QPen(Qt::black, 1));
    painter.drawRect((width() >> 1) - 3, (imageHeight_ >> 1) - 3, 7, 7);

    /// ���ƴ�ͼ�ڱ߿�
    painter.setPen(QPen(Qt::white, 2));
    painter.drawRect(2,2,width()-4, imageHeight_-4);

    /// ������߿�
    painter.setPen(QPen(Qt::black, 1));
    painter.drawRect(0,0,width()-1,height()-1);

    /// ��ǰѡ�о��εĿ����Ϣ;
    QString posInfo = QStringLiteral("POS(%1 x %2)")
            .arg(QCursor::pos().x()).arg(QCursor::pos().y());
    QFontMetrics fm = painter.fontMetrics();
    int posInfoWidth = fm.width(posInfo);

    QString colorInfo = QString("RGB(%1, %2, %3)") .arg(cursorPointColor_.red()) .arg(cursorPointColor_.green()) .arg(cursorPointColor_.blue());
    const QSize COLOR_RECT_SIZE(15, 10);
    const int SPACING = 4;
    /// �����������������
    painter.setPen(Qt::white);
    int x = SPACING;
    int y = imageHeight_ + fm.height();
    painter.drawText(QPoint(x, y), posInfo);
    y += fm.height();
    painter.drawText(QPoint(x, y), colorInfo);
}
