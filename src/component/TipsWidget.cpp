#include "TipsWidget.h"
#include <QTimer>
#include <QPainter>
#include <QMouseEvent>
#include <QApplication>
#include <QClipboard>
#include <QCursor>

static const int PADDING = 8;
static const int FONT_SIZE = 14;
static const int HEIGHT = 20;
TipsWidget::TipsWidget(QWidget *parent)
    : QLabel(parent), parent_(parent), timer_(nullptr), yOffset_(0), isClickedCopy_(false)
{
    this->setObjectName("TipsWidget");
    setAttribute(Qt::WA_DeleteOnClose, true);
    this->setAlignment(Qt::AlignCenter);
    this->setScaledContents(true);

    QStringList styleList;
    styleList << "QLabel#TipsWidget{";
    styleList << QString("height:%1px; font-size:%2px;color:rgb(255, 255, 255);").arg(HEIGHT).arg(FONT_SIZE);
    styleList << QString("padding-left:%1px; padding-right:%2px; background:rgba(50, 50, 50, 220);").arg(PADDING).arg(PADDING);
    styleList << "border: 0px solid grey; border-radius: 5px;";
    styleList << "}";
    this->setStyleSheet(styleList.join(" "));
}

TipsWidget::~TipsWidget()
{
}

void TipsWidget::autoClose(int seconds)
{
    if (!timer_) {
        timer_ = new QTimer(this);
        timer_->setSingleShot(true);
        connect(timer_, &QTimer::timeout, this, &TipsWidget::onTimeout);
    }
    timer_->setInterval(seconds * 1000);
    timer_->start();
}

void TipsWidget::setYOffset(int offset)
{
    yOffset_ = offset;
}

void TipsWidget::setClickedCopy(bool enable)
{
    isClickedCopy_ = enable;
    if (isClickedCopy_) {
        setCursor(Qt::PointingHandCursor);
    }
}

void TipsWidget::setText(const QString& text)
{
    text_ = text;
    QFont font = this->font();
    font.setPixelSize(FONT_SIZE);
    QFontMetrics fm(font);
    int textWidth = fm.width(text);
    if (textWidth < this->parentWidget()->width() - 4 * PADDING) {
        QLabel::setText(text);
    }
    else {
        textWidth = this->parentWidget()->width() - 4 * PADDING;
        QLabel::setText(fm.elidedText(text, Qt::ElideMiddle, textWidth));
    }
    this->resize(textWidth + 2*PADDING, HEIGHT);
}

QString TipsWidget::text() const
{
    return text_;
}

void TipsWidget::popup(QWidget *parent, const QString &text, int seconds, int yOffset, bool clickedCopy)
{
    if (text.isEmpty()) {
        return;
    }

    TipsWidget *widget = new TipsWidget(parent);
    widget->setText(text);
    widget->autoClose(seconds);
    widget->setYOffset(yOffset);
    widget->setClickedCopy(clickedCopy);
    widget->show();
}

void TipsWidget::showEvent(QShowEvent *event)
{
    QLabel::showEvent(event);
    QPoint p(0, 0);
    QSize size = parent_->size();
    p.setX(p.x() + (size.width() - width()) / 2);
    p.setY(p.y() + size.height() - height() - 10 + yOffset_);
    move(p);
    raise();
}

void TipsWidget::enterEvent(QEvent *event)
{
    QLabel::enterEvent(event);
    if (timer_) {
        timer_->stop();
    }
}

void TipsWidget::leaveEvent(QEvent *event)
{
    QLabel::leaveEvent(event);
    if (timer_) {
        timer_->start();
    }
}

void TipsWidget::mousePressEvent(QMouseEvent *ev)
{
    if (isClickedCopy_ && ev->button() == Qt::LeftButton) {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(this->text());
        TipsWidget::popup(parentWidget(), QStringLiteral("¸´ÖÆµ½¼ôÇÐ°å"), 1, -1 * this->height() - 5);
        return;
    }
    QLabel::mousePressEvent(ev);
}

void TipsWidget::onTimeout()
{
    this->close();
}
