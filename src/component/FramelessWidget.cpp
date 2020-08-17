#include "FramelessWidget.h"
#include <QtWidgets>
#include "common/Constants.h"

FramelessWidget::FramelessWidget(QWidget *parent)
    : QFrame(parent), 
    enableStretch_(false),
    enableEscClose_(false), 
    enableHighlight_(false), 
    isPressed_(false),
    m_dir(MIDDLE)
{
	setMouseTracking(true);
	setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setAutoFillBackground(true);
	setAttribute(Qt::WA_DeleteOnClose, true);

	title_ = new QWidget(this);
	content_ = new QWidget(this);
	QVBoxLayout *mLayout = new QVBoxLayout(this);
	mLayout->setContentsMargins(0, 0, 0, 0);
	mLayout->setSpacing(0);
	mLayout->addWidget(title_);
	mLayout->addWidget(content_, 1);
	this->setObjectName("FramelessWidget");

	this->installEventFilter(this);
}

FramelessWidget::~FramelessWidget()
{

}

void FramelessWidget::setTitle(QWidget *title)
{
	this->layout()->replaceWidget(title_, title);
	title_->deleteLater();
	title_ = title;
}

void FramelessWidget::setContent(QWidget *content)
{
	this->layout()->replaceWidget(content_, content);
	content_->deleteLater();
	content_ = content;
}

QWidget* FramelessWidget::getContent()
{
	return content_;
}

void FramelessWidget::setEnableStretch(bool enable)
{
    enableStretch_ = enable;
}
void FramelessWidget::setEnableEscClose(bool enable)
{
	enableEscClose_ = enable;
}

void FramelessWidget::mousePressEvent(QMouseEvent *event)
{
    if (this->isMaximized()) {
        event->ignore();
        return;
    }

	if (event->button() == Qt::LeftButton) {
		isPressed_ = true;
        if (m_dir != MIDDLE) {
            this->mouseGrabber();
        } else {
            movePoint_ = event->pos();
        }
	}
	event->ignore();
}

void FramelessWidget::mouseReleaseEvent(QMouseEvent *event)
{
    // 最大化后不支持窗口的移动拖拽
    if (this->isMaximized()) {
        //鼠标事件向上抛
        event->ignore();
        return;
    }

	if (event->button() == Qt::LeftButton) {
		isPressed_ = false;
        if (m_dir != MIDDLE) {
            this->releaseMouse();
            this->setCursor(QCursor(Qt::ArrowCursor));
        }
	}
	event->ignore();
}

void FramelessWidget::enterEvent(QEvent* event)
{
	isPressed_ = false;
	QFrame::enterEvent(event);
}

void FramelessWidget::leaveEvent(QEvent* event)
{
	isPressed_ = false;
	QFrame::leaveEvent(event);
}

void FramelessWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (this->isMaximized()) {
        event->ignore();
        return;
    }

	if (isPressed_) {
        if (m_dir == MIDDLE) {
            QPoint distance = event->globalPos() - movePoint_;
            if (distance.manhattanLength() > QApplication::startDragDistance()) {
                this->move(distance);
            }
        }
    } else {
        transRegion(event->globalPos());
    }
	event->ignore();
}

void FramelessWidget::keyPressEvent(QKeyEvent* event)
{
    if (enableEscClose_ && event->key() == Qt::Key_Escape) {
        this->close();
    }
    QFrame::keyPressEvent(event);
}

bool FramelessWidget::nativeEvent(const QByteArray & eventType, void * message, long * result)
{
	const MSG *msg = static_cast<MSG*>(message);
	if (!msg) {
		return false;
	}

    if (!enableStretch_) {
        m_dir = MIDDLE;
        return false;
    }

	if (msg->message == WM_LBUTTONUP) {
		isPressed_ = false;
    } else {
        const int HIT_BORDER = 5;
        if (msg->message == WM_NCHITTEST) {
            qreal ratio = this->devicePixelRatioF();
            int xPos = ((int)(short)LOWORD(msg->lParam)) / ratio - this->frameGeometry().x();
            int yPos = ((int)(short)HIWORD(msg->lParam)) / ratio - this->frameGeometry().y();

            QRect frameRect = QRect(0, 0, frameGeometry().width(), frameGeometry().height())
                .adjusted(HIT_BORDER, HIT_BORDER, -HIT_BORDER, -HIT_BORDER);

            if (xPos < frameRect.left()) {
                if (yPos < frameRect.top()) {
                    *result = HTTOPLEFT;
                } else if (yPos > frameRect.bottom()) {
                    *result = HTBOTTOMLEFT;
                } else {
                    *result = HTLEFT;
                }
            } else if (xPos > frameRect.right()) {
                if (yPos < frameRect.top()) {
                    *result = HTTOPRIGHT;
                } else if (yPos > frameRect.bottom()) {
                    *result = HTBOTTOMRIGHT;
                } else {
                    *result = HTRIGHT;
                }
            } else if (yPos < frameRect.top()) {
                *result = HTTOP;
            } else if (yPos > frameRect.bottom()) {
                *result = HTBOTTOM;
            } else {
                return false;
            }

            return true;
        }
    }
	
	return false;
}

bool FramelessWidget::eventFilter(QObject* watched, QEvent* event)
{
	if (enableHighlight_) {
		if (event->type() == QEvent::WindowActivate) {
            setActiveStyle();
		}
		else if (event->type() == QEvent::WindowDeactivate) {
            setDeactiveStyle();
		}
	}
    return false;
}

void FramelessWidget::setEnableHighlight(bool enable)
{
	enableHighlight_ = enable;
}

bool FramelessWidget::enableHightlight() const
{
    return enableHighlight_;
}

void FramelessWidget::setBackground(const QPixmap &pixmap)
{
    backgroundPixmap_ = pixmap;
}

void FramelessWidget::setActiveStyle()
{
    this->setStyleSheet(QString("QWidget#FramelessWidget{ background: transparent; border:1px solid rgb(24, 131, 215) ;}"));
}

void FramelessWidget::setDeactiveStyle()
{
    this->setStyleSheet("QWidget#FramelessWidget{ background: transparent; border: 1px solid rgb(170, 170, 170);}");
}

// 鼠标目前的位置转换对应窗口所在区域
void FramelessWidget::transRegion(const QPoint &cursorGlobalPoint)
{
    // 获取窗体在屏幕上的位置区域，tl为topleft点，rb为rightbottom点
    QPoint tl = mapToGlobal(this->rect().topLeft());
    QPoint rb = mapToGlobal(this->rect().bottomRight());
    // 当前点横坐标与纵坐标
    int x = cursorGlobalPoint.x();
    int y = cursorGlobalPoint.y();

    const int PADDING = 2;
    // 区域分析
    if (tl.x() + PADDING >= x && tl.x() <= x && tl.y() + PADDING >= y && tl.y() <= y) {
        m_dir = LEFTTOP; // 左上角
    } else if (x >= rb.x() - PADDING && x <= rb.x() && y >= rb.y() - PADDING && y <= rb.y()) {
        m_dir = RIGHTBOTTOM; // 右下角
    } else if (x <= tl.x() + PADDING && x >= tl.x() && y >= rb.y() - PADDING && y <= rb.y()) {
        m_dir = LEFTBOTTOM; //左下角
    } else if (x <= rb.x() && x >= rb.x() - PADDING && y >= tl.y() && y <= tl.y() + PADDING) {
        m_dir = RIGHTTOP; // 右上角
    } else if (x <= tl.x() + PADDING && x >= tl.x() && y >= tl.y() + PADDING && y <= rb.y() - PADDING) {
        m_dir = LEFT; // 左边
    } else if (x <= rb.x() && x >= rb.x() - PADDING  && y >= tl.y() + PADDING && y <= rb.y() - PADDING) {
        m_dir = RIGHT; // 右边
    } else if (y >= tl.y() && y <= tl.y() + PADDING  && x >= tl.x() + PADDING && x <= rb.x() - PADDING){
        m_dir = UP; // 上边
    } else if (y <= rb.y() && y >= rb.y() - PADDING  && x >= tl.x() + PADDING && x <= rb.x() - PADDING) {
        m_dir = DOWN; // 下边
    } else if (this->childAt(x, y) != NULL){
        m_dir = MIDDLE; // 默认
    } else {
        m_dir = MIDDLE; // 默认
    }
}

//void FramelessWidget::paintEvent(QPaintEvent *event)
//{
//    if (!backgroundPixmap_.isNull()) {
//        QPainter painter(this);
//        painter.drawPixmap(0, 0, this->width(), this->height(), backgroundPixmap_);
//    }
//    QFrame::paintEvent(event);
//}
