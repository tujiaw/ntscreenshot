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
#include "view/DrawPanel.h"

// 默认贴图快捷键
static QString PIN_KEY = "F6";
// 上传图片url
static QString UPLOAD_IMAGE_URL = "";
// 标记宽度
static const int MARKERT_WIDTH = 4;
// 边框感知宽度
static int BORDER_ESTHESIA_WIDTH = 6;

ScreenshotWidget::ScreenshotWidget(QWidget *parent) 
    : QWidget(parent),
    isLeftPressed_ (false), 
    darkScreen_(nullptr),
    originScreen_(nullptr), 
    selectedScreen_(nullptr), 
    drawpanel_(nullptr)
{
    // 初始化鼠标
    initCursor();
    // 初始化鼠标放大器
    initAmplifier();
    // 初始化大小感知器
    initMeasureWidget();
    // 全屏窗口
    showFullScreen();
    // 窗口与显示屏对齐
    setGeometry(getDesktopRect());
    // 置顶
    onTopMost();
    // 开启鼠标实时追踪
    setMouseTracking(true);
    // 更新鼠标的位置
    emit sigCursorPosChanged(cursor().pos().x(), cursor().pos().y());
    // 更新鼠标区域窗口
    updateMouse();
    // 展示窗口
    show();
}

ScreenshotWidget::~ScreenshotWidget(void) 
{
}

void ScreenshotWidget::pin()
{
    if (selectedScreen_) {
        selectedScreen_->onSticker();
    }
}

void ScreenshotWidget::setPinGlobalKey(const QString &key)
{
    PIN_KEY = key;
}

void ScreenshotWidget::setUploadImageUrl(const QString &url)
{
    UPLOAD_IMAGE_URL = url;
}

void ScreenshotWidget::mouseDoubleClickEvent(QMouseEvent *)
{
    emit sigDoubleClick();
}

// 初始化放大器
void ScreenshotWidget::initAmplifier(std::shared_ptr<QPixmap> originPainting) 
{
    std::shared_ptr<QPixmap>  pixmap = originPainting;
    if (!pixmap) {
        pixmap = getGlobalScreen();
    }

    amplifierTool_.reset(new AmplifierWidget(pixmap, this));
    connect(this,SIGNAL(sigCursorPosChanged(int,int)), amplifierTool_.get(), SLOT(onPositionChanged(int,int)));
    amplifierTool_->show();
    amplifierTool_->raise();
}

void ScreenshotWidget::initMeasureWidget(void)
{
    sizeTextPanel_.reset(new SelectedScreenSizeWidget(this));
    sizeTextPanel_->show();
    sizeTextPanel_->raise();
}

// 获取当前屏幕区域
QRect ScreenshotWidget::getDesktopRect(void) 
{
    return QRect(QApplication::desktop()->pos(), QApplication::desktop()->size());
}

const std::shared_ptr<QPixmap>& ScreenshotWidget::getBackgroundScreen(void) {
    if (darkScreen_) {
        return darkScreen_;
    }

    // 获得屏幕原画
    std::shared_ptr<QPixmap> screen = getGlobalScreen();

    // 制作暗色屏幕背景
    QPixmap pixmap(screen->width(), screen->height());
    pixmap.fill((QColor(0, 0, 0, 160)));
    darkScreen_.reset(new QPixmap(*screen));
    QPainter p(darkScreen_.get());
    p.drawPixmap(0, 0, pixmap);
    return darkScreen_;
}

std::shared_ptr<QPixmap> ScreenshotWidget::getGlobalScreen(void) {
    if (!originScreen_) {
        // 截取当前桌面，作为截屏的背景图
        QScreen *screen = QGuiApplication::primaryScreen();
        const QRect& rect = getDesktopRect();
        originScreen_.reset(new QPixmap(screen->grabWindow(0, rect.x(), rect.y(), rect.width(), rect.height())));
    }
    return originScreen_;
}

// 设置感知区域
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
    if (drawpanel_) {
        drawpanel_->onReferRectChanged(QRect(selectedScreen_->pos(), selectedScreen_->size()));
    }
}

void ScreenshotWidget::onSelectedScreenPosChanged(int x, int y)
{
    sizeTextPanel_->onPositionChanged(x, y);
    amplifierTool_->onPositionChanged(x, y);
    if (drawpanel_) {
        drawpanel_->onReferRectChanged(QRect(selectedScreen_->pos(), selectedScreen_->size()));
    }
}

void ScreenshotWidget::onDraw()
{
    if (selectedScreen_) {
        selectedScreen_->setDrawMode(drawpanel_->getMode());
    }
}

void ScreenshotWidget::onDrawUndo()
{
    if (selectedScreen_) {
        selectedScreen_->drawUndo();
    }
}

void ScreenshotWidget::initCursor() 
{
    QCursor cursor;
    cursor = QCursor(Util::multicolorCursorPixmap(), 15, 23);
    setCursor(cursor);
}

void ScreenshotWidget::initSelectedScreen(const QPoint &pos)
{
    // 初始化选区
    if (!selectedScreen_) {
        selectedScreen_.reset(new SelectedScreenWidget(getGlobalScreen(), pos, this));
        connect(this, SIGNAL(sigCursorPosChanged(int, int)), selectedScreen_.get(), SLOT(onCursorPosChanged(int, int)));
        connect(this, SIGNAL(sigDoubleClick()), selectedScreen_.get(), SLOT(onSaveScreen()));

        connect(selectedScreen_.get(), SIGNAL(sigSizeChanged(int, int)), this, SLOT(onSelectedScreenSizeChanged(int, int)));
        connect(selectedScreen_.get(), SIGNAL(sigPositionChanged(int, int)), this, SLOT(onSelectedScreenPosChanged(int, int)));
        connect(selectedScreen_.get(), SIGNAL(sigBorderPressed(int, int)), this, SLOT(onScreenBorderPressed(int, int)));
        connect(selectedScreen_.get(), SIGNAL(sigBorderReleased(int, int)), this, SLOT(onScreenBorderReleased(int, int)));
        connect(selectedScreen_.get(), SIGNAL(sigClose()), this, SIGNAL(sigClose()));
    }

    // 初始化绘制面板
    if (!drawpanel_) {
        drawpanel_.reset(new DrawPanel(this));
        connect(drawpanel_.get(), SIGNAL(sigStart()), this, SLOT(onDraw()));
        connect(drawpanel_.get(), SIGNAL(sigUndo()), this, SLOT(onDrawUndo()));
        connect(drawpanel_.get(), SIGNAL(sigSticker()), selectedScreen_.get(), SLOT(onSticker()));
        connect(drawpanel_.get(), SIGNAL(sigSave()), selectedScreen_.get(), SLOT(onSaveScreenOther()));
        connect(drawpanel_.get(), SIGNAL(sigFinished()), selectedScreen_.get(), SLOT(onSaveScreen()));
    }
}

void ScreenshotWidget::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton) {
        // 获得截图器当前起始位置
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
        // 选择窗口选区
        if (startPoint_ == e->pos() && !getEsthesiaRect().isEmpty()) {
            if (selectedScreen_) {
                selectedScreen_->setGeometry(getEsthesiaRect());
                selectedScreen_->show();
            }
            // 感知区域置空
            setEsthesiaRect(QRect());
        }

        // 弹出绘图面板
        if (drawpanel_ && selectedScreen_) {
            drawpanel_->onReferRectChanged(QRect(selectedScreen_->pos(), selectedScreen_->size()));
            drawpanel_->show();
            drawpanel_->raise();
        }

        // 断开鼠标移动的信号，否则鼠标选中区域还会随着鼠标的移动而改变大小
        disconnect(this, SIGNAL(sigCursorPosChanged(int, int)), selectedScreen_.get(), SLOT(onCursorPosChanged(int, int)));
        // 隐藏放大器
        amplifierTool_->hide();
        // 重置鼠标左键按下的状态
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
    /// 画全屏背景图
    const std::shared_ptr<QPixmap> &backgroundPixmap = getBackgroundScreen();
    painter.drawPixmap(0, 0, *backgroundPixmap);
    if (!getEsthesiaRect().isEmpty()) {
        /// 绘制选区
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
    /// 获取当前鼠标选中的窗口
    ::EnableWindow((HWND)winId(), FALSE);
    /// @marker: 只更新一次,可以修复用户误操作导致的查找窗口与识别界面窗口不一致.
    QRect tmpRect;
    Util::getSmallestWindowFromCursor(tmpRect);
    QPoint p = mapFromGlobal(QPoint(tmpRect.x(), tmpRect.y()));
    setEsthesiaRect(QRect(p.x(), p.y(), tmpRect.width(), tmpRect.height()));
    ::EnableWindow((HWND)winId(), TRUE);
    update();
}
    
void ScreenshotWidget::keyPressEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Escape) {
        emit sigClose();
        return;
    } else if (e->key() == Qt::Key_C) {
        if (amplifierTool_ && amplifierTool_->isVisible()) {
            QClipboard *clipboard = QApplication::clipboard();
            QColor color = amplifierTool_->getCursorPointColor();
            QString text = QString("RGB(%1, %2, %3)").arg(color.red()).arg(color.green()).arg(color.blue());
            clipboard->setText(text);
            return;
        }
    }

    if (selectedScreen_) {
        selectedScreen_->onKeyEvent(e);
    }

    e->ignore();
}

void ScreenshotWidget::keyReleaseEvent(QKeyEvent* e)
{
    if (e->modifiers() & Qt::ControlModifier) {
        if (e->key() == Qt::Key_Z) {
            if (selectedScreen_) {
                selectedScreen_->drawUndo();
            }
            return;
        }
    }
    QWidget::keyReleaseEvent(e);
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
    isDrawMode_(false)
{
    uploadImageUtil_ = new UploadImageUtil(this);
    menu_ = new QMenu(this);
    menu_->addAction(QStringLiteral("完成"), this, SLOT(onSaveScreen()), QKeySequence("Ctrl+C"));
    menu_->addAction(QStringLiteral("保存"), this, SLOT(onSaveScreenOther()), QKeySequence("Ctrl+S"));
    menu_->addAction(QStringLiteral("贴图"), this, SLOT(onSticker()), QKeySequence(PIN_KEY));
    if (!UPLOAD_IMAGE_URL.isEmpty()) {
        menu_->addAction(QStringLiteral("上传图床"), this, SLOT(onUpload()));
    }
    menu_->addSeparator();
    menu_->addAction(QStringLiteral("退出"), this, SLOT(quitScreenshot()));

    // 双击即完成
    connect(this, SIGNAL(sigDoubleClick()), this, SLOT(onSaveScreen()));

    // 开启鼠标实时追踪
    setMouseTracking(true);
    // 默认隐藏
    hide();
}

void SelectedScreenWidget::setDrawMode(const DrawMode &drawMode)
{
    drawMode_ = drawMode;
}

QPixmap SelectedScreenWidget::getPixmap()
{
    if (!originScreen_) {
        return QPixmap();
    }

    // 获取绘制的选中区域的图片
    QImage image = originScreen_->copy(currentRect_).toImage();
    QPixmap pixmap = QPixmap::fromImage(image);
    if (!drawModeCache_.isEmpty()) {
        QPainter painter(&pixmap);
        for (int i = 0; i < drawModeCache_.size(); i++) {
            DrawMode &dm = drawModeCache_[i];
            dm.draw(painter);
        }
    }
    return pixmap;
}

void SelectedScreenWidget::drawUndo()
{
    drawStartPos_ = QPoint(0, 0);
    drawEndPos_ = QPoint(0, 0);
    if (!drawModeCache_.isEmpty()) {
        drawModeCache_.pop_back();
        update();
    }
}

SelectedScreenWidget::DIRECTION SelectedScreenWidget::getRegion(const QPoint &cursor) {
    SelectedScreenWidget::DIRECTION dir = DIR_NONE;
    QPoint ptTopLeft = mapToParent(rect().topLeft());
    QPoint ptBottomRight = mapToParent(rect().bottomRight());

    int x = cursor.x();
    int y = cursor.y();
    isDrawMode_ = false;

    /// 获得鼠标当前所处窗口的边界方向
    if(ptTopLeft.x() + BORDER_ESTHESIA_WIDTH >= x && ptTopLeft.x() <= x && ptTopLeft.y() + BORDER_ESTHESIA_WIDTH >= y && ptTopLeft.y() <= y) {
        // 左上角
        dir = DIR_LEFT_TOP;
        this->setCursor(QCursor(Qt::SizeFDiagCursor));
    } else if(x >= ptBottomRight.x() - BORDER_ESTHESIA_WIDTH && x <= ptBottomRight.x() && y >= ptBottomRight.y() - BORDER_ESTHESIA_WIDTH && y <= ptBottomRight.y()) {
        // 右下角
        dir = DIR_RIGHT_BOTTOM;
        this->setCursor(QCursor(Qt::SizeFDiagCursor));
    } else if(x <= ptTopLeft.x() + BORDER_ESTHESIA_WIDTH && x >= ptTopLeft.x() && y >= ptBottomRight.y() - BORDER_ESTHESIA_WIDTH && y <= ptBottomRight.y()) {
        // 左下角
        dir = DIR_LEFT_BOTTOM;
        this->setCursor(QCursor(Qt::SizeBDiagCursor));
    } else if(x <= ptBottomRight.x() && x >= ptBottomRight.x() - BORDER_ESTHESIA_WIDTH && y >= ptTopLeft.y() && y <= ptTopLeft.y() + BORDER_ESTHESIA_WIDTH) {
        // 右上角
        dir = DIR_RIGHT_TOP;
        this->setCursor(QCursor(Qt::SizeBDiagCursor));
    } else if(x <= ptTopLeft.x() + BORDER_ESTHESIA_WIDTH && x >= ptTopLeft.x()) {
        // 左边
        dir = DIR_LEFT;
        this->setCursor(QCursor(Qt::SizeHorCursor));
    } else if( x <= ptBottomRight.x() && x >= ptBottomRight.x() - BORDER_ESTHESIA_WIDTH) {
        // 右边
        dir = DIR_RIGHT;
        this->setCursor(QCursor(Qt::SizeHorCursor));
    } else if(y >= ptTopLeft.y() && y <= ptTopLeft.y() + BORDER_ESTHESIA_WIDTH){
        // 上边
        dir = DIR_TOP;
        this->setCursor(QCursor(Qt::SizeVerCursor));
    } else if(y <= ptBottomRight.y() && y >= ptBottomRight.y() - BORDER_ESTHESIA_WIDTH) {
        // 下边
        dir = DIR_BOTTOM;
        this->setCursor(QCursor(Qt::SizeVerCursor));
    } else {
        // 默认
        dir = DIR_NONE;
        if (!drawMode_.isNone()) {
            isDrawMode_ = true;
            this->setCursor(drawMode_.cursor());
        } else {
            this->setCursor(Qt::SizeAllCursor);
        }
    }
    return dir;
}

void SelectedScreenWidget::contextMenuEvent(QContextMenuEvent *) {
    // 右键菜单
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
        drawStartPos_ = e->pos();
    }
}

void SelectedScreenWidget::mouseReleaseEvent(QMouseEvent * e) {
    if (e->button() == Qt::LeftButton) {
        if (direction_ != DIR_NONE) {
            isDrawMode_ = false;
        }

        // 当前绘制结束，存档、清理
        if (isPressed_ && isDrawMode_) {
            drawMode_.setPos(drawStartPos_, e->pos());
            if (drawMode_.isValid()) {
                drawModeCache_.push_back(drawMode_);
            }
            drawMode_.clear();
        }
        emit sigBorderReleased(e->globalX(), e->globalY());
        drawStartPos_ = QPoint(0, 0);
        drawEndPos_ = QPoint(0, 0);
        isPressed_ = false;
    }
}

void SelectedScreenWidget::mouseMoveEvent(QMouseEvent * e) {
    QPoint ptTopLeft = mapToParent(rect().topLeft());
    QPoint ptBottomLeft = mapToParent(rect().bottomLeft());
    QPoint ptBottomRight = mapToParent(rect().bottomRight());
    QPoint ptTopRight = mapToParent(rect().topRight());
    if(!isPressed_) {
        // 检查鼠标鼠标方向
        direction_ = getRegion(e->globalPos());

        // 根据方位判断拖拉对应支点
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
        // 进入绘制模式
        if (isDrawMode_) {
            drawEndPos_ = e->pos();
            // 绘制折线保存鼠标移动的每一个点
            if (drawMode_.shape() == DrawMode::PolyLine) {
                drawMode_.addPos(e->pos());
            }
            update();
            return;
        }

        if (direction_ != DIR_NONE) {
            // 鼠标在边框上拖动
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
            // 鼠标按下移动选区
            if ((e->globalPos() - movePos_).manhattanLength() > QApplication::startDragDistance()) {
                QPoint newPos = e->globalPos() - movePos_;
                if (newPos.x() < 0) {
                    newPos.setX(0);
                } else if (newPos.x() + this->width() > QApplication::desktop()->rect().width()) {
                    newPos.setX(QApplication::desktop()->rect().width() - this->width());
                }
                if (newPos.y() < 0) {
                    newPos.setY(0);
                } else if (newPos.y() + this->height() > QApplication::desktop()->rect().height()) {
                    newPos.setY(QApplication::desktop()->rect().height() - this->height());
                }
                move(newPos);
                movePos_ = e->globalPos() - pos();
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

    /// 重新计算八个锚点
    // 角点
    listMarker_.push_back(QPoint(0, 0));
    listMarker_.push_back(QPoint(width() - MARKERT_WIDTH, 0));
    listMarker_.push_back(QPoint(0, height() - MARKERT_WIDTH));
    listMarker_.push_back(QPoint(width() - MARKERT_WIDTH, height() - MARKERT_WIDTH));

    // 中点
    listMarker_.push_back(QPoint((width() >> 1), 0));
    listMarker_.push_back(QPoint((width() >> 1), height() - MARKERT_WIDTH));
    listMarker_.push_back(QPoint(0, (height() >> 1)));
    listMarker_.push_back(QPoint(width() - MARKERT_WIDTH, (height() >> 1) - MARKERT_WIDTH));

    emit sigSizeChanged(width(), height());
}

void SelectedScreenWidget::hideEvent(QHideEvent *) {
    currentRect_ = {};
    movePos_ = {};
    originPoint_ = {};
}

void SelectedScreenWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    int bw = Style::SELECTED_BORDER_WIDTH / 2;
    /// 绘制截屏编辑窗口i
    QRect pixmapRect = currentRect_;
    pixmapRect.setWidth(pixmapRect.width() - bw * 2);
    pixmapRect.setHeight(pixmapRect.height() - bw * 2);
    // 背景图往内缩bw像素，让给边线和点的绘制
    painter.drawPixmap(QPoint(bw, bw), *originScreen_, pixmapRect);
    /// 绘制边框线
    QPen pen(QColor(0, 174, 255), bw);
    painter.setPen(pen);
    painter.drawRect(rect() - QMargins(bw, bw, bw, bw));

    // 绘制边框上八个点
    QBrush brush(Qt::red);
    for (int i = 0; i < listMarker_.size(); i++) {
        QRect rect(listMarker_[i].x(), listMarker_[i].y(), MARKERT_WIDTH, MARKERT_WIDTH);
        painter.fillRect(rect, brush);
    }

    // 当前绘制
    if (isDrawMode_ && !drawMode_.isNone()) {
        drawMode_.setPos(drawStartPos_, drawEndPos_);
        drawMode_.draw(painter);
    }

    // 绘制缓存
    for (int i = 0; i < drawModeCache_.size(); i++) {
        DrawMode& dm = drawModeCache_[i];
        dm.draw(painter);
    }
}

void SelectedScreenWidget::onSaveScreenOther(void) {
    QPixmap pixmap = getPixmap();
    if (pixmap.isNull()) {
        return;
    }

    QString name = Util::pixmapUniqueName(pixmap);
    QString fileName = QFileDialog::getSaveFileName(this, QStringLiteral("保存图片"), name, "PNG (*.png)");
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

    QPoint pos(currentRect_.x(), currentRect_.y());
    quitScreenshot();
    // 使用QTimer在主屏幕退出后再弹出贴图，否则在任务栏时贴图会显示在任务栏下面
    // 等待quitScreenshot的事件完成，放弃当前环境执行，等待下一个QTimer事件再执行
    QTimer::singleShot(0, [=]() { StickerWidget::popup(pixmap, pos); });
}

void SelectedScreenWidget::onUpload()
{
    uploadImageUtil_->upload(getPixmap());
}

void SelectedScreenWidget::onSaveScreen(void) 
{
    QPixmap pixmap = getPixmap();
    if (pixmap.isNull()) {
        return;
    }

    /// 把图片放入剪切板
    QClipboard *board = QApplication::clipboard();
    board->setPixmap(pixmap);
    /// 退出当前截图工具
    quitScreenshot();
}


void SelectedScreenWidget::quitScreenshot(void) 
{
    emit sigClose();
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

    // 改变大小
    currentRect_ = QRect(rx, ry, rw, rh);
    this->setGeometry(currentRect_);
    // 改变大小后更新父窗口，防止父窗口未及时刷新而导致的问题
    parentWidget()->update();
}

