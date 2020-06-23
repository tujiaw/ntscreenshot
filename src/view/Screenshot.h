#pragma once

#include <memory>
#include <QRect>
#include <QWidget>
#include "DrawPanel.h"

class SelectedScreenWidget;
class SelectedScreenSizeWidget;
class AmplifierWidget;
class DrawPanel;
class QTimer;
class QMenu;

class ScreenshotWidget : public QWidget {
    Q_OBJECT
public:
    explicit ScreenshotWidget(QWidget *parent = 0);
    ~ScreenshotWidget(void);
    static void setPinKey(const QString &key);
    static void setUploadImageUrl(const QString &url);

signals:
    void sigReopen();
    void sigClose();
    void cursorPosChange(int, int);
    void doubleClick(void);
    void sigChildWindowRectChanged();

protected:
    virtual void mouseDoubleClickEvent(QMouseEvent*);
    virtual void mousePressEvent(QMouseEvent *);
    virtual void mouseReleaseEvent(QMouseEvent *e);
    virtual void mouseMoveEvent(QMouseEvent *e);
    virtual void keyPressEvent(QKeyEvent *e);
    virtual void keyReleaseEvent(QKeyEvent* e);
    virtual void paintEvent(QPaintEvent *);
    void updateMouse(void);

private:
    void initAmplifier(std::shared_ptr<QPixmap> originPainting = nullptr);
    void initMeasureWidget(void);
    const std::shared_ptr<QPixmap>& getBackgroundScreen(void);
    void initCursor(const QString& ico = "");
    const QRect& getDesktopRect(void);
    std::shared_ptr<QPixmap> getGlobalScreen(void);

    void setEsthesiaRect(const QRect &rect);
    const QRect& getEsthesiaRect() const;

private:

    /// 截屏窗口是否已经展示
    bool isLeftPressed_;
    /// 用于检测误操作
    QPoint startPoint_;
    /// 当前桌面屏幕的矩形数据
    QRect desktopRect_;
    /// 屏幕暗色背景图
    std::shared_ptr<QPixmap> backgroundScreen_;
    /// 屏幕原画
    std::shared_ptr<QPixmap> originPainting_;
    /// 截图屏幕
    std::shared_ptr<SelectedScreenWidget> selectedScreen_;
    std::unique_ptr<DrawPanel> drawpanel_;
    /// 截图器大小感知器
    std::shared_ptr<SelectedScreenSizeWidget> sizeTextPanel_;
    /// 放大取色器
    std::shared_ptr<AmplifierWidget> amplifierTool_;
    // 鼠标光标位置感知区域
    QRect esthesiaRect_;
    /// 置顶定时器
    QTimer *egoisticTimer_;

private slots:
    void onTopMost(void);
    void onScreenBorderPressed();
    void onScreenBorderReleased();
    void onSelectedScreenSizeChanged(int, int);
    void onSelectedScreenPosChanged(int, int);
    void onDraw();
    void onDrawUndo();
};

/**
 * @class : OESizeTextWidget
 * @brief : 大小感知器
 * @note  : 主要关乎截图器左上方的大小感知控件
*/
class SelectedScreenSizeWidget : public QWidget {
    Q_OBJECT
public:
    explicit SelectedScreenSizeWidget(QWidget *parent = 0);

protected:
    void paintEvent(QPaintEvent *);

public slots:
    void onPostionChange(int x, int y);
    void onSizeChange(int w, int h);

private:
    std::unique_ptr<QPixmap> backgroundPixmap_;
    QString                     info_;
};

class SelectedScreenWidget : public QWidget {

    Q_OBJECT

signals:
    void sizeChange(int,int);
    void postionChange(int,int);
    void doubleClick(void);
    void sigBorderPressed();
    void sigBorderReleased();
    void sigClose();

public:
    /// 内边距，决定拖拽的触发。
    const int PADDING_ = 6;

    /// 方位枚举
    enum DIRECTION {
        UPPER = 0,
        LOWER = 1,
        LEFT,
        RIGHT,
        LEFTUPPER,
        LEFTLOWER,
        RIGHTLOWER,
        RIGHTUPPER,
        NONE
    };

    explicit SelectedScreenWidget(std::shared_ptr<QPixmap> originPainting, QPoint pos, QWidget *parent = 0);
    void setDrawMode(const DrawMode &drawMode);
    QPixmap getPixmap();
    void drawUndo();

protected:
    DIRECTION getRegion(const QPoint &cursor);
    virtual void contextMenuEvent(QContextMenuEvent *);
    virtual void mouseDoubleClickEvent(QMouseEvent *e);
    virtual void mousePressEvent(QMouseEvent *e);
    virtual void mouseReleaseEvent(QMouseEvent *e);
    virtual void mouseMoveEvent(QMouseEvent *e);
    virtual void moveEvent(QMoveEvent *);
    virtual void resizeEvent(QResizeEvent *);
    virtual void hideEvent(QHideEvent *);
    virtual void enterEvent(QEvent *e);
    virtual void leaveEvent(QEvent *e);
    virtual void paintEvent(QPaintEvent *);

public slots:
    void onMouseChange(int x,int y);
    void onSticker();
    void onUpload();
    void onSaveScreen(void);
    void onSaveScreenOther(void);
    void quitScreenshot(void);

private:
    /// 窗口大小改变时，记录改变方向
    DIRECTION       direction_;
    /// 起点
    QPoint          originPoint_;
    QPoint          drawStartPos_, drawEndPos_;
    /// 鼠标是否按下
    bool            isPressed_;
    /// 拖动的距离
    QPoint          movePos_;
    /// 标记锚点
    //QVector<QPoint> listMarker_;
    QPolygon listMarker_;
    /// 屏幕原画
    std::shared_ptr<QPixmap> originPainting_;
    /// 当前窗口几何数据 用于绘制截图区域
    QRect           currentRect_;
    /// 右键菜单对象
    QMenu           *menu_;
    DrawMode drawMode_;
    QList<DrawMode> drawModeList_;
    bool isDrawMode_;
    class UploadImageUtil *uploadImageUtil_;
};

