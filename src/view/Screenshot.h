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
    void pin();
    static void setPinGlobalKey(const QString &key);
    static void setUploadImageUrl(const QString &url);

signals:
    void sigReopen();
    void sigClose();
    void sigCursorPosChanged(int, int);
    void sigDoubleClick(void);
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
    void initCursor();
    void initSelectedScreen(const QPoint &pos);
    void initAmplifier(std::shared_ptr<QPixmap> originPainting = nullptr);
    void initMeasureWidget(void);
    const std::shared_ptr<QPixmap>& getBackgroundScreen(void);
    QRect getDesktopRect(void);
    std::shared_ptr<QPixmap> getGlobalScreen(void);

    void setEsthesiaRect(const QRect &rect);
    const QRect& getEsthesiaRect() const;

private:
    // 截屏窗口是否已经展示
    bool isLeftPressed_;
    // 用于检测误操作
    QPoint startPoint_;
    // 屏幕暗色背景图
    std::shared_ptr<QPixmap> darkScreen_;
    // 屏幕原画
    std::shared_ptr<QPixmap> originScreen_;
    // 截图屏幕
    std::shared_ptr<SelectedScreenWidget> selectedScreen_;
    // 绘制面板
    std::unique_ptr<DrawPanel> drawpanel_;
    // 截图器大小感知器
    std::shared_ptr<SelectedScreenSizeWidget> sizeTextPanel_;
    // 放大取色器
    std::shared_ptr<AmplifierWidget> amplifierTool_;
    // 鼠标光标位置感知区域
    QRect esthesiaRect_;

private slots:
    void onTopMost(void);
    void onScreenBorderPressed(int, int);
    void onScreenBorderReleased(int, int);
    void onSelectedScreenSizeChanged(int, int);
    void onSelectedScreenPosChanged(int, int);
    void onDraw();
    void onDrawUndo();
};

// 显示选中区域大小
class SelectedScreenSizeWidget : public QWidget {
    Q_OBJECT
public:
    explicit SelectedScreenSizeWidget(QWidget *parent = 0);

protected:
    void paintEvent(QPaintEvent *);

public slots:
    void onPositionChanged(int x, int y);
    void onSizeChanged(int w, int h);

private:
    std::unique_ptr<QPixmap> backgroundPixmap_;
    QString info_;
};

// 选中区域
class SelectedScreenWidget : public QWidget {

    Q_OBJECT

signals:
    void sigSizeChanged(int,int);
    void sigPositionChanged(int,int);
    void sigDoubleClick(void);
    void sigBorderPressed(int,int);
    void sigBorderReleased(int,int);
    void sigClose();

public:
    // 方位枚举
    enum DIRECTION {
        DIR_NONE = 0,       // 空
        DIR_TOP = 1,        // 上
        DIR_BOTTOM,         // 下
        DIR_LEFT,           // 左
        DIR_RIGHT,          // 右
        DIR_LEFT_TOP,       // 左上
        DIR_LEFT_BOTTOM,    // 左下
        DIR_RIGHT_TOP,      // 右上
        DIR_RIGHT_BOTTOM,   // 右下
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
    void onCursorPosChanged(int x, int y);
    void onSticker();
    void onUpload();
    void onSaveScreen(void);
    void onSaveScreenOther(void);
    void onKeyEvent(QKeyEvent *e);
    void quitScreenshot(void);

private:
    // 窗口大小改变时，记录改变方向
    DIRECTION direction_;
    // 起点
    QPoint originPoint_;
    // 绘制开始位置
    QPoint drawStartPos_;
    // 绘制结束位置
    QPoint drawEndPos_;
    // 鼠标是否按下
    bool isPressed_;
    /// 拖动的距离
    QPoint movePos_;
    // 标记锚点
    QPolygon listMarker_;
    // 屏幕原画
    std::shared_ptr<QPixmap> originScreen_;
    // 当前窗口几何数据 用于绘制截图区域
    QRect currentRect_;
    // 右键菜单对象
    QMenu *menu_;
    // 绘制模式
    DrawMode drawMode_;
    // 历史绘制模式缓存
    QList<DrawMode> drawModeCache_;
    // 是否是绘制模式
    bool isDrawMode_;
    // 上传图片工具
    class UploadImageUtil *uploadImageUtil_;
};

