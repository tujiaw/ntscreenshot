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
    void pin() const;
    void setPinGlobalKey(const QString &key) const;
    void setUploadImageUrl(const QString &url) const;
    void setRgbColor(bool yes) const;

signals:
    void sigReopen();
    void sigClose();
    void sigCursorPosChanged(int, int);
    void sigDoubleClick(void);
    void sigChildWindowRectChanged();
	void sigSaveScreenshot(const QPixmap &pixmap);

protected:
    virtual void mouseDoubleClickEvent(QMouseEvent*);
    virtual void mousePressEvent(QMouseEvent *);
    virtual void mouseReleaseEvent(QMouseEvent *e);
    virtual void mouseMoveEvent(QMouseEvent *e);
    virtual void keyPressEvent(QKeyEvent *e);
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

private slots:
    void onTopMost(void);
    void onScreenBorderPressed(int, int);
    void onScreenBorderReleased(int, int);
    void onSelectedScreenSizeChanged(int, int);
    void onSelectedScreenPosChanged(int, int);

private:
    Q_DISABLE_COPY(ScreenshotWidget)
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
    // 截图器大小感知器
    std::shared_ptr<SelectedScreenSizeWidget> sizeTextPanel_;
    // 放大取色器
    std::shared_ptr<AmplifierWidget> amplifierTool_;
    // 鼠标光标位置感知区域
    QRect esthesiaRect_;
};

//////////////////////////////////////////////////////////////////////////
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
    Q_DISABLE_COPY(SelectedScreenSizeWidget)
    std::unique_ptr<QPixmap> backgroundPixmap_;
    QString info_;
};

//////////////////////////////////////////////////////////////////////////
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
	void sigSaveScreenshot(const QPixmap &pixmap);

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
    QPixmap getPixmap();
    void showDrawPanel();
    void moveDrawPanel();
    QPoint adjustPos(QPoint p);
    QRect adjustRect(QRect r);
	void setCurrentRect(const QRect &rect);

protected:
    void updateCursorDir(const QPoint &cursor);
    virtual void contextMenuEvent(QContextMenuEvent *);
    virtual void mouseDoubleClickEvent(QMouseEvent *e);
    virtual void mousePressEvent(QMouseEvent *e);
    virtual void mouseReleaseEvent(QMouseEvent *e);
    virtual void mouseMoveEvent(QMouseEvent *e);
    virtual void moveEvent(QMoveEvent *);
    virtual void resizeEvent(QResizeEvent *);
    virtual void hideEvent(QHideEvent *);
    virtual void paintEvent(QPaintEvent *);

public slots:
    void onSelectRectChanged(int left, int top, int right ,int bottom);
    void onCursorPosChanged(int x, int y);
    void onSticker();
    void onUpload();
    void onOcr();
    void onSaveScreen(void);
    void onSaveScreenOther(void);
    void onKeyEvent(QKeyEvent *e);
    void quitScreenshot(void);

private:
    Q_DISABLE_COPY(SelectedScreenWidget)
    // 窗口大小改变时，记录改变方向
    DIRECTION direction_;
    // 起点
    QPoint originPoint_;
    // 鼠标是否按下
    bool isPressed_;
    // 拖动的距离
    QPoint movePos_;
    // 标记锚点
    QPolygon listMarker_;
    // 屏幕原画
    std::shared_ptr<QPixmap> originScreen_;
    // 选中区域窗口rect用于绘制截图区域
    QRect currentRect_;
    // 右键菜单对象
    QMenu *menu_;
    // 绘图面板
    DrawPanel drawPanel_;
    // 上传图片工具
    class UploadImageUtil *uploadImageUtil_;
    // OCR
    class Ocr* ocr_;
};

