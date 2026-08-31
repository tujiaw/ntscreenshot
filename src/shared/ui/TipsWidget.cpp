#include "TipsWidget.h"
#include <QTimer>
#include <QPainter>
#include <QMouseEvent>
#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QEnterEvent>
#include <QScreen>

static const int PADDING = 8;
static const int FONT_SIZE = 12;
static const int HEIGHT = 20;

TipsWidget::TipsWidget(QWidget *parent)
    : QLabel(parent), parent_(parent), timer_(nullptr), yOffset_(0), isClickedCopy_(false)
{
    this->setObjectName("TipsWidget");
    setAttribute(Qt::WA_DeleteOnClose, true);
    
    if (!parent) {
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        setAttribute(Qt::WA_TranslucentBackground);
    }
    
    this->setAlignment(Qt::AlignCenter);
    this->setScaledContents(true);
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
    int textWidth = fm.horizontalAdvance(text);

    int maxWidth = 400;
    if (this->parentWidget()) {
        maxWidth = this->parentWidget()->width() - 4 * PADDING;
    } else if (QGuiApplication::primaryScreen()) {
        maxWidth = QGuiApplication::primaryScreen()->availableGeometry().width() / 3;
    }

    if (textWidth < maxWidth) {
        QLabel::setText(text);
    }
    else {
        textWidth = maxWidth;
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

    if (!parent) {
        parent = QApplication::activeWindow();
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
    
    QSize parentSize;
    QPoint parentPos(0, 0);

    if (parent_) {
        parentSize = parent_->size();
    } else {
        QScreen *screen = QGuiApplication::primaryScreen();
        if (screen) {
            QRect screenGeom = screen->availableGeometry();
            parentSize = screenGeom.size();
            parentPos = screenGeom.topLeft();
        }
    }

    int x = (parentSize.width() - width()) / 2;
    int y = parentSize.height() - height() - 10 + yOffset_;

    if (!parent_) {
        x += parentPos.x();
        y += parentPos.y();
    }
    
    move(x, y);
    raise();
}

void TipsWidget::enterEvent(QEnterEvent *event)
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
        TipsWidget::popup(parentWidget(), QStringLiteral("复制到剪切板"), 1, -1 * this->height() - 5);
        return;
    }
    QLabel::mousePressEvent(ev);
}

void TipsWidget::onTimeout()
{
    this->close();
}
