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

/// 鼠标按钮图片的十六进制数据
static const unsigned char uc_mouse_image[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,  0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52
    ,0x00, 0x00, 0x00, 0x1D, 0x00, 0x00, 0x00, 0x2D,  0x08, 0x06, 0x00, 0x00, 0x00, 0x52, 0xE9, 0x60
    ,0xA2, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59,  0x73, 0x00, 0x00, 0x0B, 0x13, 0x00, 0x00, 0x0B
    ,0x13, 0x01, 0x00, 0x9A, 0x9C, 0x18, 0x00, 0x00,  0x01, 0x40, 0x49, 0x44, 0x41, 0x54, 0x58, 0x85
    ,0xED, 0xD5, 0x21, 0x6E, 0xC3, 0x30, 0x14, 0xC6,  0xF1, 0xFF, 0x9B, 0xC6, 0x36, 0x30, 0x38, 0xA9
    ,0x05, 0x01, 0x05, 0x81, 0x05, 0x03, 0x39, 0xCA,  0x60, 0x8F, 0xD2, 0x03, 0xEC, 0x10, 0x3B, 0x46
    ,0xC1, 0xC0, 0xC6, 0x0A, 0x3B, 0x96, 0xB1, 0x80,  0x82, 0xC1, 0x56, 0x2A, 0xFF, 0x06, 0xE2, 0x36
    ,0x75, 0x9A, 0xB4, 0xCA, 0xEC, 0x4E, 0x9A, 0xE4,  0x2F, 0xB2, 0x42, 0x22, 0xFF, 0xF2, 0xFC, 0x9C
    ,0x18, 0x52, 0x52, 0x52, 0x52, 0x52, 0x52, 0x52,  0x52, 0x52, 0x52, 0xFE, 0x55, 0xE4, 0xC6, 0xA0
    ,0xDC, 0xC4, 0x71, 0x87, 0xC1, 0xC1, 0x68, 0x01,  0xCC, 0x06, 0xC2, 0x51, 0xD0, 0x29, 0xB0, 0x18
    ,0x00, 0xDF, 0xC6, 0x40, 0x33, 0x37, 0x84, 0x30,  0x4C, 0x80, 0x85, 0xCE, 0x7B, 0x2E, 0x2A, 0x91
    ,0x84, 0x24, 0xBE, 0x25, 0xDE, 0x25, 0x5E, 0x2F,  0x6E, 0xAE, 0xD0, 0x37, 0x92, 0x10, 0xF0, 0x09
    ,0x54, 0x40, 0xE9, 0xEE, 0x15, 0xC6, 0xA2, 0x77,  0xFE, 0xE0, 0xE5, 0x85, 0x8F, 0x16, 0x58, 0xDF
    ,0x35, 0x06, 0x5B, 0xD3, 0xB9, 0xD4, 0x11, 0xD0,  0xA5, 0x8F, 0xDE, 0x57, 0x75, 0x83, 0x73, 0x50
    ,0x06, 0xF6, 0x72, 0x0A, 0x47, 0x40, 0x57, 0x0D,  0x38, 0xDE, 0xC0, 0x04, 0x6F, 0x68, 0x05, 0x36
    ,0xF5, 0xE1, 0x08, 0x3D, 0xCD, 0xEA, 0xEA, 0x5A,  0xD8, 0xBE, 0x5A, 0x46, 0xB0, 0x05, 0x1E, 0xAC
    ,0xF1, 0xC2, 0xD1, 0xCC, 0x01, 0x6D, 0x74, 0x02,  0xDB, 0x3B, 0xBF, 0xD3, 0x73, 0x07, 0x87, 0x2F
    ,0xEF, 0x53, 0x07, 0x38, 0x82, 0x2F, 0xF6, 0xFB,  0xB8, 0x81, 0x73, 0x41, 0x69, 0x28, 0x3A, 0x7A
    ,0x5C, 0xDD, 0x73, 0xCF, 0x3A, 0x86, 0xA3, 0x05,  0x87, 0xEA, 0xCC, 0x60, 0xA1, 0x06, 0x75, 0x89
    ,0xFE, 0x77, 0x92, 0x76, 0x68, 0x23, 0xEF, 0x88,  0xD3, 0x4C, 0xA8, 0x10, 0x7A, 0xD4, 0xEF, 0x8E
    ,0xBE, 0x8B, 0x68, 0x79, 0x3A, 0xB1, 0x72, 0xE1,  0xAE, 0xBC, 0x13, 0x0D, 0xDE, 0xBD, 0x3D, 0xF3
    ,0x08, 0x15, 0xD4, 0xDF, 0x4C, 0x06, 0x36, 0xF7,  0x9E, 0x09, 0xED, 0xE9, 0x99, 0x97, 0x3E, 0x42
    ,0xFF, 0x30, 0x42, 0x4B, 0xA1, 0x8D, 0xD8, 0xE9,  0x2A, 0xBD, 0xED, 0x41, 0x25, 0x2A, 0x89, 0x37
    ,0x1F, 0xBD, 0xEA, 0x61, 0x8B, 0x5F, 0xDD, 0xC1,  0xFA, 0x01, 0xD8, 0xA3, 0x8F, 0xFB, 0xCA, 0x70
    ,0x16, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45,  0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82
};

static QString PIN_KEY = "F6";
static QString UPLOAD_IMAGE_URL = "";

const int MARKERT_WIDTH = 4;

ScreenshotWidget::ScreenshotWidget(QWidget *parent) 
    : QWidget(parent),
    isLeftPressed_ (false), 
    backgroundScreen_(nullptr),
    originPainting_(nullptr), 
    selectedScreen_(nullptr), 
    drawpanel_(nullptr)
{
    /// 初始化鼠标
    initCursor();
    /// 初始化鼠标放大器
    initAmplifier();
    /// 初始化大小感知器
    initMeasureWidget();
    /// 全屏窗口
    showFullScreen();
    /// 窗口与显示屏对齐
    setGeometry(getDesktopRect());
    /// 霸道置顶
    onTopMost();
    /// 开启鼠标实时追踪
    setMouseTracking(true);
    /// 更新鼠标的位置
    emit cursorPosChange(cursor().pos().x(), cursor().pos().y());
    /// 更新鼠标区域窗口
    updateMouse();
    /// 展示窗口
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

void ScreenshotWidget::mouseDoubleClickEvent(QMouseEvent *) {
    emit doubleClick();
}


/**
 * 初始化放大镜 (色彩采集器)
 */
void ScreenshotWidget::initAmplifier(std::shared_ptr<QPixmap> originPainting) {
    std::shared_ptr<QPixmap>  pixmap = originPainting;
    if (!pixmap) {
        pixmap = getGlobalScreen();
    }

    amplifierTool_.reset(new AmplifierWidget(pixmap, this));
    connect(this,SIGNAL(cursorPosChange(int,int)), amplifierTool_.get(), SLOT(onPostionChange(int,int)));
    amplifierTool_->show();
    amplifierTool_->raise();
}

void ScreenshotWidget::initMeasureWidget(void)
{
    sizeTextPanel_.reset(new SelectedScreenSizeWidget(this));
    sizeTextPanel_->show();
    sizeTextPanel_->raise();
}

/**
 * 功能：获得当前屏幕的大小
 */
const QRect &ScreenshotWidget::getDesktopRect(void) {
    if (!desktopRect_.isEmpty()) {
        return desktopRect_;
    }
    /// 兼容多个屏幕的问题
    desktopRect_ = QRect(QApplication::desktop()->pos(), QApplication::desktop()->size());
    return desktopRect_;
}

const std::shared_ptr<QPixmap>& ScreenshotWidget::getBackgroundScreen(void) {
    if (backgroundScreen_) {
        return backgroundScreen_;
    }

    /// 获得屏幕原画
    std::shared_ptr<QPixmap> screen = getGlobalScreen();

    /// 制作暗色屏幕背景
    QPixmap pixmap(screen->width(), screen->height());
    pixmap.fill((QColor(0, 0, 0, 160)));
    backgroundScreen_.reset(new QPixmap(*screen));
    QPainter p(backgroundScreen_.get());
    p.drawPixmap(0, 0, pixmap);
    return backgroundScreen_;
}

std::shared_ptr<QPixmap> ScreenshotWidget::getGlobalScreen(void) {
    if (!originPainting_) {
        /// 截取当前桌面，作为截屏的背景图
        QScreen *screen = QGuiApplication::primaryScreen();
        const QRect& rect = getDesktopRect();
        originPainting_.reset(new QPixmap(screen->grabWindow(0, rect.x(), rect.y(), rect.width(), rect.height())));
    }
    return originPainting_;
}

void ScreenshotWidget::setEsthesiaRect(const QRect &rect)
{
    if (rect != esthesiaRect_) {
        esthesiaRect_ = rect;
        if (!rect.isEmpty()) {
            sizeTextPanel_->onPostionChange(rect.x(), rect.y());
            sizeTextPanel_->onSizeChange(rect.width(), rect.height());
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

void ScreenshotWidget::onScreenBorderPressed()
{
    if (amplifierTool_->isHidden()) {
        amplifierTool_->show();
        amplifierTool_->raise();
    }
}

void ScreenshotWidget::onScreenBorderReleased()
{
    if (amplifierTool_->isVisible()) {
        amplifierTool_->hide();
    }
}

void ScreenshotWidget::onSelectedScreenSizeChanged(int w, int h)
{
    sizeTextPanel_->onSizeChange(w, h);
    amplifierTool_->onSizeChange(w, h);
    if (drawpanel_) {
        drawpanel_->onReferRectChanged(QRect(selectedScreen_->pos(), selectedScreen_->size()));
    }
}

void ScreenshotWidget::onSelectedScreenPosChanged(int x, int y)
{
    sizeTextPanel_->onPostionChange(x, y);
    amplifierTool_->onPostionChange(x, y);
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

/*
 * 初始化鼠标
 * 参数：_ico 鼠标图片的资源文件
 */
void ScreenshotWidget::initCursor(const QString& ico) {
    QPixmap pixmap;
    if (ico.isEmpty()) {
        pixmap.loadFromData(uc_mouse_image, sizeof(uc_mouse_image));
    } else {
        pixmap.load(ico);
    }
    QCursor cursor;
    cursor = QCursor(pixmap, 15, 23);
    setCursor(cursor);
}

void ScreenshotWidget::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton) {
        // 获得截图器当前起始位置
        startPoint_ = e->pos();
        isLeftPressed_ = true;

        if (!selectedScreen_) {
            selectedScreen_.reset(new SelectedScreenWidget(getGlobalScreen(), e->pos(), this));
            connect(this, SIGNAL(cursorPosChange(int, int)), selectedScreen_.get(), SLOT(onMouseChange(int, int)));
            connect(this, SIGNAL(doubleClick()), selectedScreen_.get(), SLOT(onSaveScreen()));

            connect(selectedScreen_.get(), SIGNAL(sizeChange(int, int)), this, SLOT(onSelectedScreenSizeChanged(int, int)));
            connect(selectedScreen_.get(), SIGNAL(postionChange(int, int)), this, SLOT(onSelectedScreenPosChanged(int, int)));
            connect(selectedScreen_.get(), SIGNAL(sigBorderPressed()), this, SLOT(onScreenBorderPressed()));
            connect(selectedScreen_.get(), SIGNAL(sigBorderReleased()), this, SLOT(onScreenBorderReleased()));
            connect(selectedScreen_.get(), SIGNAL(sigClose()), this, SIGNAL(sigClose()));
        }

        if (!drawpanel_) {
            drawpanel_.reset(new DrawPanel(this));
            connect(drawpanel_.get(), SIGNAL(sigStart()), this, SLOT(onDraw()));
            connect(drawpanel_.get(), SIGNAL(sigUndo()), this, SLOT(onDrawUndo()));
            connect(drawpanel_.get(), SIGNAL(sigSticker()), selectedScreen_.get(), SLOT(onSticker()));
            connect(drawpanel_.get(), SIGNAL(sigSave()), selectedScreen_.get(), SLOT(onSaveScreenOther()));
            connect(drawpanel_.get(), SIGNAL(sigFinished()), selectedScreen_.get(), SLOT(onSaveScreen()));
        }
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
        disconnect(this, SIGNAL(cursorPosChange(int, int)), selectedScreen_.get(), SLOT(onMouseChange(int, int)));
        // 隐藏放大器
        amplifierTool_->hide();
        // 重置鼠标左键按下的状态
        isLeftPressed_ = false;
    }
    QWidget::mouseReleaseEvent(e);
}

void ScreenshotWidget::mouseMoveEvent(QMouseEvent *e) {
    emit cursorPosChange(e->x(), e->y());
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

void SelectedScreenSizeWidget::onPostionChange(int x, int y) {
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    const int& ry = y - height() - 1;
    if (ry < 0) {
        this->raise();
    }
    move(x, ((ry < 0) ? y : ry));
    show();
}

void SelectedScreenSizeWidget::onSizeChange(int w, int h) {
    info_ = QString("%1 x %2").arg(w).arg(h);
}

///////////////////////////////////////////////////////////
SelectedScreenWidget::SelectedScreenWidget(std::shared_ptr<QPixmap> originPainting, QPoint pos, QWidget *parent) 
    : QWidget(parent), 
    direction_(NONE), 
    originPoint_(pos),
    isPressed_(false), 
    originPainting_(originPainting),
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

    /// 双击即完成
    connect(this, SIGNAL(doubleClick()), this, SLOT(onSaveScreen()));

    /// 开启鼠标实时追踪
    setMouseTracking(true);
    /// 默认隐藏
    hide();
}

void SelectedScreenWidget::setDrawMode(const DrawMode &drawMode)
{
    drawMode_ = drawMode;
}

QPixmap SelectedScreenWidget::getPixmap()
{
    if (originPainting_) {
        QImage image = originPainting_->copy(currentRect_).toImage();
        if (drawModeList_.isEmpty()) {
            return QPixmap::fromImage(image);
        } else {
            QPixmap pixmap = QPixmap::fromImage(image);
            QPainter painter(&pixmap);
            for (int i = 0; i < drawModeList_.size(); i++) {
                DrawMode &dm = drawModeList_[i];
                dm.draw(painter);
            }
            return pixmap;
        }
    }
    return QPixmap();
}

void SelectedScreenWidget::drawUndo()
{
    drawStartPos_ = QPoint(0, 0);
    drawEndPos_ = QPoint(0, 0);
    if (!drawModeList_.isEmpty()) {
        drawModeList_.pop_back();
        update();
    }
}

SelectedScreenWidget::DIRECTION SelectedScreenWidget::getRegion(const QPoint &cursor) {
    SelectedScreenWidget::DIRECTION ret_dir = NONE;
    // left upper
    QPoint pt_lu = mapToParent(rect().topLeft());
    // right lower
    QPoint pt_rl = mapToParent(rect().bottomRight());

    int x = cursor.x();
    int y = cursor.y();
    isDrawMode_ = false;

    /// 获得鼠标当前所处窗口的边界方向
    if(pt_lu.x() + PADDING_ >= x && pt_lu.x() <= x && pt_lu.y() + PADDING_ >= y && pt_lu.y() <= y) {
        // 左上角
        ret_dir = LEFTUPPER;
        this->setCursor(QCursor(Qt::SizeFDiagCursor));
    } else if(x >= pt_rl.x() - PADDING_ && x <= pt_rl.x() && y >= pt_rl.y() - PADDING_ && y <= pt_rl.y()) {
        // 右下角
        ret_dir = RIGHTLOWER;
        this->setCursor(QCursor(Qt::SizeFDiagCursor));
    } else if(x <= pt_lu.x() + PADDING_ && x >= pt_lu.x() && y >= pt_rl.y() - PADDING_ && y <= pt_rl.y()) {
        // 左下角
        ret_dir = LEFTLOWER;
        this->setCursor(QCursor(Qt::SizeBDiagCursor));
    } else if(x <= pt_rl.x() && x >= pt_rl.x() - PADDING_ && y >= pt_lu.y() && y <= pt_lu.y() + PADDING_) {
        // 右上角
        ret_dir = RIGHTUPPER;
        this->setCursor(QCursor(Qt::SizeBDiagCursor));
    } else if(x <= pt_lu.x() + PADDING_ && x >= pt_lu.x()) {
        // 左边
        ret_dir = LEFT;
        this->setCursor(QCursor(Qt::SizeHorCursor));
    } else if( x <= pt_rl.x() && x >= pt_rl.x() - PADDING_) {
        // 右边
        ret_dir = RIGHT;
        this->setCursor(QCursor(Qt::SizeHorCursor));
    } else if(y >= pt_lu.y() && y <= pt_lu.y() + PADDING_){
        // 上边
        ret_dir = UPPER;
        this->setCursor(QCursor(Qt::SizeVerCursor));
    } else if(y <= pt_rl.y() && y >= pt_rl.y() - PADDING_) {
        // 下边
        ret_dir = LOWER;
        this->setCursor(QCursor(Qt::SizeVerCursor));
    } else {
        // 默认
        ret_dir = NONE;
        if (!drawMode_.isNone()) {
            isDrawMode_ = true;
        }
        this->setCursor(isDrawMode_ ? Qt::CrossCursor : Qt::SizeAllCursor);
    }
    return ret_dir;
}

void SelectedScreenWidget::contextMenuEvent(QContextMenuEvent *) {
    /// 在鼠标位置弹射出菜单栏
    menu_->exec(cursor().pos());
}

void SelectedScreenWidget::mouseDoubleClickEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton) {
        emit doubleClick();
        e->accept();
    }
}

void SelectedScreenWidget::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton) {
        isPressed_ = true;
        if(direction_ != NONE) {
            this->mouseGrabber();
            emit sigBorderPressed();
        }
        /// @bug :这里可能存在问题, 不应当使用globalPos
        movePos_ = e->globalPos() - pos();
        drawStartPos_ = e->pos();
    }
}

void SelectedScreenWidget::mouseReleaseEvent(QMouseEvent * e) {
    if (e->button() == Qt::LeftButton) {
        if(direction_ != NONE) {
            isDrawMode_ = false;
        }

        if (isPressed_ && isDrawMode_) {
            drawMode_.setPos(drawStartPos_, e->pos());
            if (drawMode_.isValid()) {
                drawModeList_.push_back(drawMode_);
            }
            drawMode_.clear();
        }
        emit sigBorderReleased();
        drawStartPos_ = QPoint(0, 0);
        drawEndPos_ = QPoint(0, 0);
        isPressed_ = false;
    }
}

void SelectedScreenWidget::mouseMoveEvent(QMouseEvent * e) {
    QPoint gloPoint = mapToParent(e->pos());
    // left upper
    QPoint pt_lu = mapToParent(rect().topLeft());
    // left lower
    QPoint pt_ll = mapToParent(rect().bottomLeft());
    // right lower
    QPoint pt_rl = mapToParent(rect().bottomRight());
    // right upper
    QPoint pt_ru = mapToParent(rect().topRight());
    if(!isPressed_) {
        /// 检查鼠标鼠标方向
        direction_ = getRegion(gloPoint);

        /// 根据方位判断拖拉对应支点
        switch(direction_) {
        case NONE:
        case RIGHT:
        case RIGHTLOWER:
            originPoint_ = pt_lu;
            break;
        case RIGHTUPPER:
            originPoint_ = pt_ll;
            break;
        case LEFT:
        case LEFTLOWER:
            originPoint_ = pt_ru;
            break;
        case LEFTUPPER:
        case UPPER:
            originPoint_ = pt_rl;
            break;
        case LOWER:
            originPoint_ = pt_lu;
            break;
        }
    }
    else {
        if (isDrawMode_) {
            drawEndPos_ = e->pos();
            if (drawMode_.shape() == DrawMode::PolyLine) {
                drawMode_.addPos(e->pos());
            }
            update();
            return;
        }

        if(direction_ != NONE) {
            const int& global_x = gloPoint.x();
            /// 鼠标进行拖拉拽
            switch(direction_) {
            case LEFT:
                return onMouseChange(global_x, pt_ll.y() + 1);
            case RIGHT:
                return onMouseChange(global_x, pt_rl.y() + 1);
            case UPPER:
                return onMouseChange(pt_lu.x(), gloPoint.y());
            case LOWER:
                return onMouseChange(pt_rl.x() + 1, gloPoint.y());
            case LEFTUPPER:
            case RIGHTUPPER:
            case LEFTLOWER:
            case RIGHTLOWER:
                return onMouseChange(global_x, gloPoint.y());
            default:
                break;
            }
        }
        else {
            // 移动选区
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
    emit postionChange(x(), y());
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

    emit sizeChange(width(), height());
}

void SelectedScreenWidget::hideEvent(QHideEvent *) {
    currentRect_ = {};
    movePos_ = {};
    originPoint_ = {};
}

void SelectedScreenWidget::enterEvent(QEvent *e) {
    setCursor(isDrawMode_ ? Qt::CrossCursor : Qt::SizeAllCursor);
    QWidget::enterEvent(e);
}

void SelectedScreenWidget::leaveEvent(QEvent *e) {
    setCursor(Qt::ArrowCursor);
    QWidget::leaveEvent(e);
}


void SelectedScreenWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    int bw = Style::SELECTED_BORDER_WIDTH / 2;
    /// 绘制截屏编辑窗口i
    QRect pixmapRect = currentRect_;
    pixmapRect.setWidth(pixmapRect.width() - bw * 2);
    pixmapRect.setHeight(pixmapRect.height() - bw * 2);
    // 背景图往内缩bw像素，让给边线和点的绘制
    painter.drawPixmap(QPoint(bw, bw), *originPainting_, pixmapRect);
    /// 绘制边框线
    QPen pen(QColor(0, 174, 255), bw);
    painter.setPen(pen);
    painter.drawRect(rect() - QMargins(bw, bw, bw, bw));

    // 绘制八个点
    //改变点的宽度
    QBrush brush(Qt::red);
    for (int i = 0; i < listMarker_.size(); i++) {
        QRect rect(listMarker_[i].x(), listMarker_[i].y(), MARKERT_WIDTH, MARKERT_WIDTH);
        painter.fillRect(rect, brush);
    }

    if (isDrawMode_ && !drawMode_.isNone()) {
        drawMode_.setPos(drawStartPos_, drawEndPos_);
        drawMode_.draw(painter);
    }

    for (int i = 0; i < drawModeList_.size(); i++) {
        DrawMode& dm = drawModeList_[i];
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

    qDebug() << "key:" << key;
    QList<QAction*> actions = menu_->actions();
    for (int i = 0; i < actions.size(); i++) {
        QKeySequence seq = actions[i]->shortcut();
        if (!seq.isEmpty() && seq == key) {
            qDebug() << "seq:" << seq;
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

void SelectedScreenWidget::onSaveScreen(void) {
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


void SelectedScreenWidget::quitScreenshot(void) {
    emit sigClose();
}

void SelectedScreenWidget::onMouseChange(int x, int y) {
    show();
    if (x < 0 || y < 0) {
        return;
    }

    const int& rx = (x >= originPoint_.x()) ? originPoint_.x() : x;
    const int& ry = (y >= originPoint_.y()) ? originPoint_.y() : y;
    const int& rw = abs(x - originPoint_.x());
    const int& rh = abs(y - originPoint_.y());

    /// 改变大小
    currentRect_ = QRect(rx, ry, rw, rh);
    this->setGeometry(currentRect_);
    /// 改变大小后更新父窗口，防止父窗口未及时刷新而导致的问题
    parentWidget()->update();
}

