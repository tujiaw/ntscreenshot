#include "Screenshot.h"
#include <QDesktopWidget>
#include <QApplication>
#include <QMouseEvent>
#include <QFileDialog>
#include <QClipboard>
#include <QDateTime>
#include <QPainter>
#include <QScreen>
#include <QCursor>
#include <QMutex>
#include <QMenu>
#include <QPen>
#include <QBrush>
#include <QTimer>
#include <QDebug>

#ifdef Q_OS_WIN32
#include <windows.h>
#endif

#include "common/Constants.h"
#include "common/Util.h"
#include "view/Amplifier.h"
#include "view/Stickers.h"

// Ĭ����ͼ��ݼ�
static QString PIN_KEY = "F6";
// �ϴ�ͼƬurl
static QString UPLOAD_IMAGE_URL = "";
// ��ǿ��
static const int MARKERT_WIDTH = 4;
// �߿��֪���
static int BORDER_ESTHESIA_WIDTH = 6;
// ��ɫ��ʾRGB��ʽ
static bool IS_RGB_COLOR = true;
// ����͸��ɫ
const QColor BACKGROUND_COLOR(0, 0, 0, 160);

ScreenshotWidget::ScreenshotWidget(QWidget *parent) 
    : QWidget(parent),
    isLeftPressed_ (false), 
    darkScreen_(nullptr),
    originScreen_(nullptr), 
    selectedScreen_(nullptr)
{
    // ��ʼ�����
    initCursor();
    // ��ʼ�����Ŵ���
    initAmplifier();
    // ��ʼ����С��֪��
    initMeasureWidget();
    // ȫ������
    showFullScreen();
    // ��������ʾ������
    setGeometry(getDesktopRect());
    // �ö�
    onTopMost();
    // �������ʵʱ׷��
    setMouseTracking(true);
    // ��������λ��
    emit sigCursorPosChanged(cursor().pos().x(), cursor().pos().y());
    // ����������򴰿�
    updateMouse();
    // չʾ����
    show();
}

ScreenshotWidget::~ScreenshotWidget(void) 
{
}

void ScreenshotWidget::pin() const
{
    if (selectedScreen_) {
        selectedScreen_->onSticker();
    }
}

void ScreenshotWidget::setPinGlobalKey(const QString &key) const
{
    PIN_KEY = key;
}

void ScreenshotWidget::setUploadImageUrl(const QString &url) const
{
    UPLOAD_IMAGE_URL = url;
}

void ScreenshotWidget::setRgbColor(bool yes) const
{
    IS_RGB_COLOR = yes;
	if (amplifierTool_) {
		amplifierTool_->setRgbColor(IS_RGB_COLOR);
	}
}

void ScreenshotWidget::mouseDoubleClickEvent(QMouseEvent *)
{
    emit sigDoubleClick();
}

// ��ʼ���Ŵ���
void ScreenshotWidget::initAmplifier(std::shared_ptr<QPixmap> originPainting) 
{
    std::shared_ptr<QPixmap>  pixmap = originPainting;
    if (!pixmap) {
        pixmap = getGlobalScreen();
    }

    amplifierTool_.reset(new AmplifierWidget(pixmap, this));
    connect(this,SIGNAL(sigCursorPosChanged(int,int)), amplifierTool_.get(), SLOT(onPositionChanged(int,int)));
    amplifierTool_->setRgbColor(IS_RGB_COLOR);
    amplifierTool_->show();
    amplifierTool_->raise();
}

void ScreenshotWidget::initMeasureWidget(void)
{
    sizeTextPanel_.reset(new SelectedScreenSizeWidget(this));
    sizeTextPanel_->show();
    sizeTextPanel_->raise();
}

// ��ȡ��ǰ��Ļ����
QRect ScreenshotWidget::getDesktopRect(void) 
{
    return QRect(QApplication::desktop()->pos(), QApplication::desktop()->size());
}

const std::shared_ptr<QPixmap>& ScreenshotWidget::getBackgroundScreen(void) {
    if (darkScreen_) {
        return darkScreen_;
    }

    // �����Ļԭ��
    std::shared_ptr<QPixmap> screen = getGlobalScreen();

    // ������ɫ��Ļ����
    QPixmap pixmap(screen->width(), screen->height());
    pixmap.fill(BACKGROUND_COLOR); // ����͸����Ĭ��160
    darkScreen_.reset(new QPixmap(*screen));
    QPainter p(darkScreen_.get());
    p.drawPixmap(0, 0, pixmap);
    return darkScreen_;
}

std::shared_ptr<QPixmap> ScreenshotWidget::getGlobalScreen(void) {
    if (!originScreen_) {
        // ��ȡ��ǰ���棬��Ϊ�����ı���ͼ
        const QRect& rect = getDesktopRect();
        originScreen_.reset(new QPixmap(QGuiApplication::primaryScreen()->grabWindow(0, rect.x(), rect.y(), rect.width(), rect.height())));
    }
    return originScreen_;
}

// ���ø�֪����
void ScreenshotWidget::setEsthesiaRect(const QRect &rect)
{
    if (rect != esthesiaRect_) {
        esthesiaRect_ = rect;
        if (!rect.isEmpty()) {
            sizeTextPanel_->onPositionChanged(rect.x(), rect.y());
            sizeTextPanel_->onSizeChanged(rect.width(), rect.height());
        }
    }
}

const QRect& ScreenshotWidget::getEsthesiaRect() const
{
    return esthesiaRect_;
}

void ScreenshotWidget::onTopMost(void)
{
#ifdef Q_OS_WIN32
    SetWindowPos((HWND)this->winId(),HWND_TOPMOST,this->pos().x(),this->pos().y(),this->width(),this->height(),SWP_SHOWWINDOW);
#else
    Qt::WindowFlags flags = windowFlags();
    flags |= Qt::WindowStaysOnTopHint;
    setWindowFlags(flags);
#endif
}

void ScreenshotWidget::onScreenBorderPressed(int x, int y)
{
    if (amplifierTool_->isHidden()) {
        amplifierTool_->move(x, y);
        amplifierTool_->show();
        amplifierTool_->raise();
    }
}

void ScreenshotWidget::onScreenBorderReleased(int x, int y)
{
    if (amplifierTool_->isVisible()) {
        amplifierTool_->hide();
    }
}

void ScreenshotWidget::onSelectedScreenSizeChanged(int w, int h)
{
    sizeTextPanel_->onSizeChanged(w, h);
    amplifierTool_->onSizeChanged(w, h);
}

void ScreenshotWidget::onSelectedScreenPosChanged(int x, int y)
{
    sizeTextPanel_->onPositionChanged(x, y);
    amplifierTool_->onPositionChanged(x, y);
}

void ScreenshotWidget::initCursor() 
{
    QCursor cursor;
    cursor = QCursor(Util::multicolorCursorPixmap(), 15, 23);
    setCursor(cursor);
}

void ScreenshotWidget::initSelectedScreen(const QPoint &pos)
{
    // ��ʼ��ѡ��
    if (!selectedScreen_) {
        selectedScreen_.reset(new SelectedScreenWidget(getGlobalScreen(), pos, this));
        connect(this, SIGNAL(sigCursorPosChanged(int, int)), selectedScreen_.get(), SLOT(onCursorPosChanged(int, int)));
        connect(this, SIGNAL(sigDoubleClick()), selectedScreen_.get(), SLOT(onSaveScreen()));

        connect(selectedScreen_.get(), SIGNAL(sigSizeChanged(int, int)), this, SLOT(onSelectedScreenSizeChanged(int, int)));
        connect(selectedScreen_.get(), SIGNAL(sigPositionChanged(int, int)), this, SLOT(onSelectedScreenPosChanged(int, int)));
        connect(selectedScreen_.get(), SIGNAL(sigBorderPressed(int, int)), this, SLOT(onScreenBorderPressed(int, int)));
        connect(selectedScreen_.get(), SIGNAL(sigBorderReleased(int, int)), this, SLOT(onScreenBorderReleased(int, int)));
        connect(selectedScreen_.get(), SIGNAL(sigClose()), this, SIGNAL(sigClose()));
		connect(selectedScreen_.get(), SIGNAL(sigSaveScreenshot(const QPixmap&)), this, SIGNAL(sigSaveScreenshot(const QPixmap&)));
    }
}

void ScreenshotWidget::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton) {
        // ��ý�ͼ����ǰ��ʼλ��
        startPoint_ = e->pos();
        isLeftPressed_ = true;
        initSelectedScreen(e->pos());
    }
    QWidget::mousePressEvent(e);
}

void ScreenshotWidget::mouseReleaseEvent(QMouseEvent *e) {
    if (e->button() == Qt::RightButton) {
        if (selectedScreen_) {
            emit sigReopen();
        } else {
            emit sigClose();
        }
        return ;
    }
    else if (isLeftPressed_ == true && e->button() == Qt::LeftButton) {
        // ѡ�񴰿�ѡ��
        if (startPoint_ == e->pos() && !getEsthesiaRect().isEmpty()) {
            if (selectedScreen_) {
				selectedScreen_->setCurrentRect(getEsthesiaRect());
                selectedScreen_->show();
            }
            // ��֪�����ÿ�
            setEsthesiaRect(QRect());
        }

        // ������ͼ���
        if (selectedScreen_) {
            selectedScreen_->showDrawPanel();
        }

        // �Ͽ�����ƶ����źţ��������ѡ�����򻹻����������ƶ����ı��С
        disconnect(this, SIGNAL(sigCursorPosChanged(int, int)), selectedScreen_.get(), SLOT(onCursorPosChanged(int, int)));
        // ���طŴ���
        amplifierTool_->hide();
        // �������������µ�״̬
        isLeftPressed_ = false;
    }
    QWidget::mouseReleaseEvent(e);
}

void ScreenshotWidget::mouseMoveEvent(QMouseEvent *e) {
    emit sigCursorPosChanged(e->x(), e->y());
    if (isLeftPressed_) {
        amplifierTool_->raise();
        setEsthesiaRect(QRect());
        update();
    }
    else if (!isLeftPressed_ && (!selectedScreen_ || selectedScreen_->isHidden())){
        onTopMost();
        updateMouse();
    }
    QWidget::mouseMoveEvent(e);
}

void ScreenshotWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    /// ��ȫ������ͼ
    const std::shared_ptr<QPixmap> &backgroundPixmap = getBackgroundScreen();
    painter.drawPixmap(0, 0, *backgroundPixmap);
    if (!getEsthesiaRect().isEmpty()) {
        /// ����ѡ��
        QPen pen = painter.pen();
        pen.setColor(Style::SELECTED_BORDER_COLOR);
        pen.setWidth(Style::SELECTED_BORDER_WIDTH);
        painter.setPen(pen);
        int padding = Style::SELECTED_BORDER_WIDTH / 2;
        QRect borderRect = getEsthesiaRect() - QMargins(padding, padding, padding, padding);
        painter.drawRect(borderRect);
        QRect imageRect = borderRect - QMargins(padding, padding, padding, padding);
        painter.drawPixmap(QPoint(imageRect.x(), imageRect.y()), *getGlobalScreen(), imageRect);
    }
}

void ScreenshotWidget::updateMouse(void) {
    /// ��ȡ��ǰ���ѡ�еĴ���
    ::EnableWindow((HWND)winId(), FALSE);
    /// ֻ����һ��,�����޸��û���������µĲ��Ҵ�����ʶ����洰�ڲ�һ��.
    QRect tmpRect;
    Util::getSmallestWindowFromCursor(tmpRect);
    QPoint p = mapFromGlobal(QPoint(tmpRect.x(), tmpRect.y()));
    setEsthesiaRect(QRect(p.x(), p.y(), tmpRect.width(), tmpRect.height()));
    ::EnableWindow((HWND)winId(), TRUE);
    update();
}
    
void ScreenshotWidget::keyPressEvent(QKeyEvent *e) {
    int left = 0, top = 0, right = 0, bottom = 0;
    if (e->key() == Qt::Key_Escape) {
        emit sigClose();
        return;
	}
	else if (e->key() == Qt::Key_Equal) {
		if (selectedScreen_) {
			selectedScreen_->plusRect();
			selectedScreen_->moveDrawPanel();
			return;
		}
	}
	else if (e->key() == Qt::Key_Minus) {
		if (selectedScreen_) {		
			selectedScreen_->minusRect();
			selectedScreen_->moveDrawPanel();
			return;
		}
	}
	else if (e->key() == Qt::Key_C) {
        if (amplifierTool_ && amplifierTool_->isVisible()) {
            QClipboard *clipboard = QApplication::clipboard();
            clipboard->setText(amplifierTool_->getCursorPointColor());
            return;
        }
    } else if (e->key() == Qt::Key_Left) {
        if (e->modifiers() & Qt::ControlModifier) {
            left = -1;
            right = 0;
        } else if (e->modifiers() & Qt::ShiftModifier) {
            left = 0;
            right = -1;
        } else {
            left = -1;
            right = -1;
        }
    } else if (e->key() == Qt::Key_Right) {
        if (e->modifiers() & Qt::ControlModifier) {
            left = 0;
            right = 1;
        } else if (e->modifiers() & Qt::ShiftModifier) {
            left = 1;
            right = 0;
        } else {
            left = 1;
            right = 1;
        }
    } else if (e->key() == Qt::Key_Up) {
        if (e->modifiers() & Qt::ControlModifier) {
            top = -1;
            bottom = 0;
        } else if (e->modifiers() & Qt::ShiftModifier) {
            top = 0;
            bottom = -1;
        } else {
            top = -1;
            bottom = -1;
        }
    } else if (e->key() == Qt::Key_Down) {
        if (e->modifiers() & Qt::ControlModifier) {
            top = 0;
            bottom = 1;
        } else if (e->modifiers() & Qt::ShiftModifier) {
            top = 1;
            bottom = 0;
        } else {
            top = 1;
            bottom = 1;
        }
    }

    if (selectedScreen_) {
        selectedScreen_->onKeyEvent(e);
        selectedScreen_->onSelectRectChanged(left, top, right, bottom);
        selectedScreen_->moveDrawPanel();
    } else {
        QPoint p(0, 0);
        if (left != 0) {
            p.setX(left);
        } else if (right != 0) {
            p.setX(right);
        } else if (top != 0) {
            p.setY(top);
        } else if (bottom != 0) {
            p.setY(bottom);
        }
        if (!p.isNull()) {
            QCursor cursor;
            cursor.setPos(cursor.pos() + p);
        }
    }

    e->ignore();
}

///////////////////////////////////////////////////////////
SelectedScreenSizeWidget::SelectedScreenSizeWidget(QWidget *parent) : QWidget(parent) 
{
    setFixedSize(Style::OERECT_FIXED_SIZE);
    backgroundPixmap_.reset(new QPixmap(width(),height()));
    backgroundPixmap_->fill(Style::OERECT_BACKGROUND);
    hide();
}

void SelectedScreenSizeWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.drawPixmap(rect(),*backgroundPixmap_);
    painter.setPen(QPen(QColor(Qt::white)));
    painter.drawText(rect(), Qt::AlignLeft | Qt::AlignBottom, info_);
}

void SelectedScreenSizeWidget::onPositionChanged(int x, int y) {
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    const int& ry = y - height() - 1;
    if (ry < 0) {
        this->raise();
    }
    move(x, ((ry < 0) ? y : ry));
    show();
}

void SelectedScreenSizeWidget::onSizeChanged(int w, int h) {
    info_ = QString("%1 x %2").arg(w).arg(h);
}

///////////////////////////////////////////////////////////
SelectedScreenWidget::SelectedScreenWidget(std::shared_ptr<QPixmap> originPainting, QPoint pos, QWidget *parent) 
    : QWidget(parent), 
    direction_(DIR_NONE), 
    originPoint_(pos),
    isPressed_(false), 
    originScreen_(originPainting),
    drawPanel_(parent, this)
{
    drawPanel_.hide();
    connect(&drawPanel_, SIGNAL(sigSticker()), this, SLOT(onSticker()));
    connect(&drawPanel_, SIGNAL(sigSave()), this, SLOT(onSaveScreenOther()));
    connect(&drawPanel_, SIGNAL(sigFinished()), this, SLOT(onSaveScreen()));

    //uploadImageUtil_ = new UploadImageUtil(this);
    menu_ = new QMenu(this);
    menu_->addAction(QStringLiteral("���"), this, SLOT(onSaveScreen()), QKeySequence("Ctrl+C"));
    menu_->addAction(QStringLiteral("����"), this, SLOT(onSaveScreenOther()), QKeySequence("Ctrl+S"));
    menu_->addAction(QStringLiteral("��ͼ"), this, SLOT(onSticker()), QKeySequence(PIN_KEY));
    menu_->addAction(QStringLiteral("����"), drawPanel_.drawer(), &Drawer::undo, QKeySequence("Ctrl+Z"));
    //if (!UPLOAD_IMAGE_URL.isEmpty()) {
    //    menu_->addAction(QStringLiteral("�ϴ�ͼ��"), this, SLOT(onUpload()));
    //}
    //menu_->addAction(QStringLiteral("OCR"), this, SLOT(onOcr()), QKeySequence("Ctrl+O"));
    menu_->addSeparator();
    menu_->addAction(QStringLiteral("�˳�"), this, SLOT(quitScreenshot()));

    // ˫�������
    connect(this, SIGNAL(sigDoubleClick()), this, SLOT(onSaveScreen()));
    // �������ʵʱ׷��
    setMouseTracking(true);
    // ǿ�ƽ��㣬Ϊ�˲�������¼�
    setFocusPolicy(Qt::StrongFocus);
    // Ĭ������
    hide();
}

QPixmap SelectedScreenWidget::getPixmap()
{
    if (!originScreen_) {
        return QPixmap();
    }

    // ��ȡ���Ƶ�ѡ�������ͼƬ
    QImage image = originScreen_->copy(currentRect_).toImage();
    QPixmap pixmap = QPixmap::fromImage(image);
    drawPanel_.drawer()->drawPixmap(pixmap);
    return pixmap;
}

void SelectedScreenWidget::showDrawPanel()
{
    moveDrawPanel();
    drawPanel_.show();
    drawPanel_.raise();
}

void SelectedScreenWidget::moveDrawPanel()
{
    drawPanel_.onReferRectChanged(QRect(this->pos(), this->size()));
}

QPoint SelectedScreenWidget::adjustPos(QPoint p)
{
    if (p.x() < 0) {
        p.setX(0);
    } else if (p.x() + this->width() > QApplication::desktop()->rect().width()) {
        p.setX(QApplication::desktop()->rect().width() - this->width());
    }
    if (p.y() < 0) {
        p.setY(0);
    } else if (p.y() + this->height() > QApplication::desktop()->rect().height()) {
        p.setY(QApplication::desktop()->rect().height() - this->height());
    }
    return p;
}

void SelectedScreenWidget::setCurrentRect(const QRect &rect)
{
	currentRect_ = rect;
	setGeometry(rect);
}

QRect SelectedScreenWidget::currentRect() const
{
	return currentRect_;
}

void SelectedScreenWidget::minusRect()
{
	scaledRect(1);
}

void SelectedScreenWidget::plusRect()
{
	scaledRect(-1);
}

void SelectedScreenWidget::scaledRect(int direction)
{
	if (!originScreen_) {
		return;
	}

	QImage image = originScreen_->toImage();
	int w = currentRect_.width();
	int h = currentRect_.height();
	int left = currentRect_.left();
	int right = currentRect_.right();
	int top = currentRect_.top();
	int bottom = currentRect_.bottom();

	auto getLinePixel = [&image](const QPoint &from, QPoint &to) -> QRgb {
		QMap<QRgb, int> mp;
		for (int i = from.x(); i <= to.x(); i++) {
			for (int j = from.y(); j <= to.y(); j++) {
				if (image.rect().contains(i, j)) {
					mp[image.pixel(i, j)]++;
				}
			}
		}
		QRgb rgb = 0;
		int count = 0;
		QMapIterator<QRgb, int> i(mp);
		while (i.hasNext()) {
			i.next();
			if (i.value() > count) {
				count = i.value();
				rgb = i.key();
			}
		}
		return rgb;
	};

	int offsetLeft = 0;
	int offsetRight = 0;
	int offsetTop = 0;
	int offsetBottom = 0;

	// left
	QRgb clr = getLinePixel(QPoint(left, top), QPoint(left, bottom));
	if (clr > 0) {
		for (int i = 1; i <= image.width(); i++) {
			int offset = i * direction;
			QRgb tmpClr = getLinePixel(QPoint(left + offset, top), QPoint(left + offset, bottom));
			if (tmpClr == 0) break;
			if (clr != tmpClr) {
				offsetLeft = offset;
				break;
			}
		}
	}

	// right
	clr = getLinePixel(QPoint(right, top), QPoint(right, bottom));
	if (clr > 0) {
		for (int i = 1; i <= image.width(); i++) {
			int offset = -1 * i * direction;
			QRgb tmpClr = getLinePixel(QPoint(right + offset, top), QPoint(right + offset, bottom));
			if (tmpClr == 0) break;
			if (clr != tmpClr) {
				offsetRight = offset;
				break;
			}
		}
	}

	// top
	clr = getLinePixel(QPoint(left, top), QPoint(right, top));
	if (clr > 0) {
		for (int i = 1; i <= image.height(); i++) {
			int offset = i * direction;
			QRgb tmpClr = getLinePixel(QPoint(left, top + offset), QPoint(right, top + offset));
			if (tmpClr == 0) break;
			if (clr != tmpClr) {
				offsetTop = offset;
				break;
			}
		}
	}

	// bottom
	clr = getLinePixel(QPoint(left, bottom), QPoint(right, bottom));
	if (clr > 0) {
		for (int i = 1; i <= image.height(); i++) {
			int offset = -1 * i * direction;
			QRgb tmpClr = getLinePixel(QPoint(left, bottom + offset), QPoint(right, bottom + offset));
			if (tmpClr == 0) break;
			if (clr != tmpClr) {
				offsetBottom = offset;
				break;
			}
		}
	}

	onSelectRectChanged(offsetLeft, offsetTop, offsetRight, offsetBottom);
}

QRect SelectedScreenWidget::adjustRect(QRect r)
{
    if (r.left() < 0) {
        r.setLeft(0);
    }
    if (r.right() > QApplication::desktop()->rect().width()) {
        r.setRight(QApplication::desktop()->rect().width());
    }
    if (r.top() < 0) {
        r.setTop(0);
    }
    if (r.bottom() > QApplication::desktop()->rect().height()) {
        r.setBottom(QApplication::desktop()->rect().height());
    }
    return r;
}

void SelectedScreenWidget::updateCursorDir(const QPoint &cursor) {
    SelectedScreenWidget::DIRECTION dir = DIR_NONE;
    QPoint ptTopLeft = mapToParent(rect().topLeft());
    QPoint ptBottomRight = mapToParent(rect().bottomRight());

    int x = cursor.x();
    int y = cursor.y();
    /// �����굱ǰ�������ڵı߽緽��
    if(ptTopLeft.x() + BORDER_ESTHESIA_WIDTH >= x && ptTopLeft.x() <= x && ptTopLeft.y() + BORDER_ESTHESIA_WIDTH >= y && ptTopLeft.y() <= y) {
        // ���Ͻ�
        dir = DIR_LEFT_TOP;
        this->setCursor(QCursor(Qt::SizeFDiagCursor));
    } else if(x >= ptBottomRight.x() - BORDER_ESTHESIA_WIDTH && x <= ptBottomRight.x() && y >= ptBottomRight.y() - BORDER_ESTHESIA_WIDTH && y <= ptBottomRight.y()) {
        // ���½�
        dir = DIR_RIGHT_BOTTOM;
        this->setCursor(QCursor(Qt::SizeFDiagCursor));
    } else if(x <= ptTopLeft.x() + BORDER_ESTHESIA_WIDTH && x >= ptTopLeft.x() && y >= ptBottomRight.y() - BORDER_ESTHESIA_WIDTH && y <= ptBottomRight.y()) {
        // ���½�
        dir = DIR_LEFT_BOTTOM;
        this->setCursor(QCursor(Qt::SizeBDiagCursor));
    } else if(x <= ptBottomRight.x() && x >= ptBottomRight.x() - BORDER_ESTHESIA_WIDTH && y >= ptTopLeft.y() && y <= ptTopLeft.y() + BORDER_ESTHESIA_WIDTH) {
        // ���Ͻ�
        dir = DIR_RIGHT_TOP;
        this->setCursor(QCursor(Qt::SizeBDiagCursor));
    } else if(x <= ptTopLeft.x() + BORDER_ESTHESIA_WIDTH && x >= ptTopLeft.x()) {
        // ���
        dir = DIR_LEFT;
        this->setCursor(QCursor(Qt::SizeHorCursor));
    } else if( x <= ptBottomRight.x() && x >= ptBottomRight.x() - BORDER_ESTHESIA_WIDTH) {
        // �ұ�
        dir = DIR_RIGHT;
        this->setCursor(QCursor(Qt::SizeHorCursor));
    } else if(y >= ptTopLeft.y() && y <= ptTopLeft.y() + BORDER_ESTHESIA_WIDTH){
        // �ϱ�
        dir = DIR_TOP;
        this->setCursor(QCursor(Qt::SizeVerCursor));
    } else if(y <= ptBottomRight.y() && y >= ptBottomRight.y() - BORDER_ESTHESIA_WIDTH) {
        // �±�
        dir = DIR_BOTTOM;
        this->setCursor(QCursor(Qt::SizeVerCursor));
    } else {
        // Ĭ��
        dir = DIR_NONE;
    }
    direction_ = dir;
    drawPanel_.drawer()->setEnable(direction_ == DIR_NONE);
}

void SelectedScreenWidget::contextMenuEvent(QContextMenuEvent *) {
    // �Ҽ��˵�
    menu_->exec(cursor().pos());
}

void SelectedScreenWidget::mouseDoubleClickEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton) {
        emit sigDoubleClick();
        e->accept();
    }
}

void SelectedScreenWidget::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton) {
        isPressed_ = true;
        if (direction_ != DIR_NONE) {
            this->mouseGrabber();
            emit sigBorderPressed(e->globalX(), e->globalY());
        }
        movePos_ = e->globalPos() - pos();
    }
}

void SelectedScreenWidget::mouseReleaseEvent(QMouseEvent * e) {
    if (e->button() == Qt::LeftButton) {
		isPressed_ = false;
        emit sigBorderReleased(e->globalX(), e->globalY());
    }
}

void SelectedScreenWidget::mouseMoveEvent(QMouseEvent * e) {
    QPoint ptTopLeft = mapToParent(rect().topLeft());
    QPoint ptBottomLeft = mapToParent(rect().bottomLeft());
    QPoint ptBottomRight = mapToParent(rect().bottomRight());
    QPoint ptTopRight = mapToParent(rect().topRight());
    if(!isPressed_) {
        updateCursorDir(e->globalPos());
        // ���ݷ�λ�ж�������Ӧ֧��
        switch(direction_) {
        case DIR_NONE:
        case DIR_RIGHT:
        case DIR_RIGHT_BOTTOM:
            originPoint_ = ptTopLeft;
            break;
        case DIR_RIGHT_TOP:
            originPoint_ = ptBottomLeft;
            break;
        case DIR_LEFT:
        case DIR_LEFT_BOTTOM:
            originPoint_ = ptTopRight;
            break;
        case DIR_LEFT_TOP:
        case DIR_TOP:
            originPoint_ = ptBottomRight;
            break;
        case DIR_BOTTOM:
            originPoint_ = ptTopLeft;
            break;
        }
    }
    else {
        if (direction_ != DIR_NONE) {
            // ����ڱ߿����϶�
            switch(direction_) {
            case DIR_LEFT:
                return onCursorPosChanged(e->globalX(), ptBottomLeft.y() + 1);
            case DIR_RIGHT:
                return onCursorPosChanged(e->globalX(), ptBottomRight.y() + 1);
            case DIR_TOP:
                return onCursorPosChanged(ptTopLeft.x(), e->globalY());
            case DIR_BOTTOM:
                return onCursorPosChanged(ptBottomRight.x() + 1, e->globalY());
            case DIR_LEFT_TOP:
            case DIR_RIGHT_TOP:
            case DIR_LEFT_BOTTOM:
            case DIR_RIGHT_BOTTOM:
                return onCursorPosChanged(e->globalX(), e->globalY());
            default:
                break;
            }
        }
        else {
            // ��갴���ƶ�ѡ��
            if ((e->globalPos() - movePos_).manhattanLength() > QApplication::startDragDistance()) {
                QPoint newPos = e->globalPos() - movePos_;
                move(adjustPos(newPos));
                movePos_ = e->globalPos() - pos();
                moveDrawPanel();
            }
        }
    }
    currentRect_ = geometry();
}

void SelectedScreenWidget::moveEvent(QMoveEvent *) 
{
    emit sigPositionChanged(x(), y());
}

void SelectedScreenWidget::resizeEvent(QResizeEvent *) 
{
    listMarker_.clear();

    /// ���¼���˸�ê��
    // �ǵ�
    listMarker_.push_back(QPoint(0, 0));
    listMarker_.push_back(QPoint(width() - MARKERT_WIDTH, 0));
    listMarker_.push_back(QPoint(0, height() - MARKERT_WIDTH));
    listMarker_.push_back(QPoint(width() - MARKERT_WIDTH, height() - MARKERT_WIDTH));

    // �е�
    listMarker_.push_back(QPoint((width() >> 1), 0));
    listMarker_.push_back(QPoint((width() >> 1), height() - MARKERT_WIDTH));
    listMarker_.push_back(QPoint(0, (height() >> 1)));
    listMarker_.push_back(QPoint(width() - MARKERT_WIDTH, (height() >> 1) - MARKERT_WIDTH));

    moveDrawPanel();
    emit sigSizeChanged(width(), height());
}

void SelectedScreenWidget::hideEvent(QHideEvent *) 
{
    currentRect_ = {};
    movePos_ = {};
    originPoint_ = {};
}

void SelectedScreenWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    int bw = Style::SELECTED_BORDER_WIDTH / 2;
    /// ���ƽ����༭����i
    QRect pixmapRect = currentRect_;
    pixmapRect.setWidth(pixmapRect.width() - bw);
    pixmapRect.setHeight(pixmapRect.height() - bw);
    // ����ͼ������bw���أ��ø����ߺ͵�Ļ���
    painter.drawPixmap(QPoint(0, 0), *originScreen_, pixmapRect);
    /// ���Ʊ߿���
    QPen pen(QColor(0, 174, 255), bw);
    painter.setPen(pen);
    painter.drawRect(rect() - QMargins(bw, bw, bw, bw));
    /// ����ɫ�߿����Ϊ����ɫ
    QPen pen2(BACKGROUND_COLOR, bw);
    painter.setPen(pen2);
    painter.drawRect(rect() + QMargins(0, 0, bw, bw));

    // ���Ʊ߿��ϰ˸���
    QBrush brush(Qt::red);
    for (int i = 0; i < listMarker_.size(); i++) {
        QRect rect(listMarker_[i].x(), listMarker_[i].y(), MARKERT_WIDTH, MARKERT_WIDTH);
        painter.fillRect(rect, brush);
    }

    drawPanel_.drawer()->onPaint(painter);
}

void SelectedScreenWidget::onSaveScreenOther(void) {
    QPixmap pixmap = getPixmap();
    if (pixmap.isNull()) {
        return;
    }

    QString name = Util::pixmapName();
    QString fileName = QFileDialog::getSaveFileName(this, QStringLiteral("����ͼƬ"), name, "PNG (*.png)");
    if (fileName.length() > 0) {
        pixmap.save(fileName, "png");
        quitScreenshot();
    }
}

void SelectedScreenWidget::onKeyEvent(QKeyEvent *e)
{
    QString key = Util::strKeyEvent(e);
    if (key.isEmpty()) {
        return;
    }

    QList<QAction*> actions = menu_->actions();
    for (int i = 0; i < actions.size(); i++) {
        QKeySequence seq = actions[i]->shortcut();
        if (!seq.isEmpty() && seq == key) {
            emit actions[i]->triggered();
            break;
        }
    }
}

void SelectedScreenWidget::onSticker()
{
    QPixmap pixmap = getPixmap();
    if (pixmap.isNull()) {
        return;
    }

	emit sigSaveScreenshot(pixmap);

    QPoint pos(currentRect_.x(), currentRect_.y());
    if (StickerWidget::hasBorder()) {
        pos -= QPoint(1, 1);
    }
    
    quitScreenshot();
    // ʹ��QTimer������Ļ�˳����ٵ�����ͼ��������������ʱ��ͼ����ʾ������������
    // �ȴ�quitScreenshot���¼���ɣ�������ǰ����ִ�У��ȴ���һ��QTimer�¼���ִ��
    QTimer::singleShot(0, [=]() { StickerWidget::popup(pixmap, pos); });
}

void SelectedScreenWidget::onSaveScreen(void) 
{
    QPixmap pixmap = getPixmap();
    if (pixmap.isNull()) {
        return;
    }

    /// ��ͼƬ������а�
    QClipboard *board = QApplication::clipboard();
    board->setPixmap(pixmap);

	emit sigSaveScreenshot(pixmap);

    /// �˳���ǰ��ͼ����
    quitScreenshot();
}

void SelectedScreenWidget::quitScreenshot(void) 
{
    emit sigClose();
}

void SelectedScreenWidget::onSelectRectChanged(int left, int top, int right, int bottom)
{
    QRect newRect;
    newRect.setLeft(currentRect_.left() + left);
    newRect.setRight(currentRect_.right() + right);
    newRect.setTop(currentRect_.top() + top);
    newRect.setBottom(currentRect_.bottom() + bottom);
    if (newRect.width() <= 0 || newRect.height() <= 0) {
        return;
    }

    if (adjustRect(newRect) == newRect) {
		this->setCurrentRect(newRect);
        // �ı��С����¸����ڣ���ֹ������δ��ʱˢ�¶����µ�����
        parentWidget()->update();
    }
}

void SelectedScreenWidget::onCursorPosChanged(int x, int y) 
{
    if (isHidden()) {
        show();
    }

    if (x < 0 || y < 0) {
        return;
    }

    int rx = (x >= originPoint_.x()) ? originPoint_.x() : x;
    int ry = (y >= originPoint_.y()) ? originPoint_.y() : y;
    int rw = abs(x - originPoint_.x());
    int rh = abs(y - originPoint_.y());

    // �ı��С
	this->setCurrentRect(QRect(rx, ry, rw, rh));
    // �ı��С����¸����ڣ���ֹ������δ��ʱˢ�¶����µ�����
    parentWidget()->update();
}

