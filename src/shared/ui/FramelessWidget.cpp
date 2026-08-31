#include "FramelessWidget.h"
#include <QtWidgets>
#include <QEnterEvent>
#include "core/foundation/Constants.h"
#include "core/theme/ThemeManager.h"

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

FramelessWidget::FramelessWidget(QWidget *parent)
    : QFrame(parent), 
    enableStretch_(false),
    stretchBorderWidth_(6),
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
    if (!title || title == title_) {
        return;
    }
    // 确保所有权归当前窗口（layout 通常也会重设 parent，但这里显式处理更安全）
    if (title->parentWidget() != this) {
        title->setParent(this);
    }
	QLayoutItem* oldItem = this->layout()->replaceWidget(title_, title);
	delete oldItem;
	title_->deleteLater();
	title_ = title;
}

void FramelessWidget::setContent(QWidget *content)
{
    if (!content || content == content_) {
        return;
    }
    if (content->parentWidget() != this) {
        content->setParent(this);
    }
	QLayoutItem* oldItem = this->layout()->replaceWidget(content_, content);
	delete oldItem;
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

void FramelessWidget::setStretchBorderWidth(int width)
{
    stretchBorderWidth_ = qMax(2, width);
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

void FramelessWidget::enterEvent(QEnterEvent* event)
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

bool FramelessWidget::nativeEvent(const QByteArray & eventType, void * message, qintptr * result)
{
#ifdef Q_OS_WIN
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
        if (msg->message == WM_NCHITTEST) {
            const int hitBorder = qMax(2, stretchBorderWidth_);
            qreal ratio = this->devicePixelRatioF();
            int xPos = ((int)(short)LOWORD(msg->lParam)) / ratio - this->frameGeometry().x();
            int yPos = ((int)(short)HIWORD(msg->lParam)) / ratio - this->frameGeometry().y();

            QRect frameRect = QRect(0, 0, frameGeometry().width(), frameGeometry().height())
                .adjusted(hitBorder, hitBorder, -hitBorder, -hitBorder);

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
#else
	return QFrame::nativeEvent(eventType, message, result);
#endif
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
    this->setStyleSheet(QString("QWidget#FramelessWidget{ background: transparent; border:1px solid %1;}").arg(
        ThemeManager::tokens().accent.name()));
}

void FramelessWidget::setDeactiveStyle()
{
    this->setStyleSheet(QString("QWidget#FramelessWidget{ background: transparent; border: 1px solid %1;}").arg(
        ThemeManager::tokens().border.name()));
}

// 鼠标目前的位置转换对应窗口所在区域
void FramelessWidget::transRegion(const QPoint &cursorGlobalPoint)
{
    if (!enableStretch_) {
        m_dir = MIDDLE;
        unsetCursor();
        return;
    }

    // 获取窗体在屏幕上的位置区域
    const QRect r = this->rect();
    const QPoint tl = mapToGlobal(r.topLeft());
    const QPoint rb = mapToGlobal(r.bottomRight());
    const int x = cursorGlobalPoint.x();
    const int y = cursorGlobalPoint.y();
    const int padding = qMax(2, stretchBorderWidth_);

    auto inRange = [](int val, int min, int max) { return val >= min && val <= max; };
    
    bool l = inRange(x, tl.x(), tl.x() + padding);
    bool r_side = inRange(x, rb.x() - padding, rb.x());
    bool t = inRange(y, tl.y(), tl.y() + padding);
    bool b = inRange(y, rb.y() - padding, rb.y());
    bool x_in = inRange(x, tl.x(), rb.x());
    bool y_in = inRange(y, tl.y(), rb.y());

    m_dir = MIDDLE;
    if (l && t) m_dir = LEFTTOP;
    else if (r_side && b) m_dir = RIGHTBOTTOM;
    else if (l && b) m_dir = LEFTBOTTOM;
    else if (r_side && t) m_dir = RIGHTTOP;
    else if (l && y_in) m_dir = LEFT;
    else if (r_side && y_in) m_dir = RIGHT;
    else if (t && x_in) m_dir = UP;
    else if (b && x_in) m_dir = DOWN;

    switch (m_dir) {
    case LEFT:
    case RIGHT:
        setCursor(Qt::SizeHorCursor);
        break;
    case UP:
    case DOWN:
        setCursor(Qt::SizeVerCursor);
        break;
    case LEFTTOP:
    case RIGHTBOTTOM:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case RIGHTTOP:
    case LEFTBOTTOM:
        setCursor(Qt::SizeBDiagCursor);
        break;
    case MIDDLE:
    default:
        unsetCursor();
        break;
    }
}
