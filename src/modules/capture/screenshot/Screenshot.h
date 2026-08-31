#pragma once

#include <memory>
#include <QRect>
#include <QWidget>
#include <QPolygon>
#include <QElapsedTimer>

class QMenu;
class QTimer;
class IRectDetector;
class SelectedScreenSizeWidget;
class AmplifierWidget;
class ScreenshotActionController;
class WindowManager;

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

enum class ScreenState {
    Exploring,   // 探测中（鼠标移动，吸附窗口，放大镜跟随）
    Selecting,   // 框选中（鼠标按下拖拽，生成初始选区）
    Editing      // 编辑中（选区确定，弹出工具栏，可以拉伸边框或涂鸦）
};

class ScreenshotWidget : public QWidget {
    Q_OBJECT
public:
    explicit ScreenshotWidget(WindowManager* windowManager, QWidget *parent = 0);
    ~ScreenshotWidget(void);
    void pin() const;
    void setPinGlobalKey(const QString &key) const;
    void setRgbColor(bool yes) const;
    void setBackgroundColorAlpha(int alpha);

signals:
    void sigReopen();
    void sigClose();
    void sigCursorPosChanged(int, int);
    void sigDoubleClick(void);
    void sigChildWindowRectChanged();
	void sigSaveScreenshot(const QPixmap &pixmap);
    void sigLongScreenshotRequested(const QRect &captureRect);
    void sigGifRecordingRequested(const QRect &captureRect);

public:
    void scaledRect(int direction);
    void updateCursorDir(const QPoint &cursor);
    void moveDrawPanel();
    QPoint adjustPos(QPoint p);
    QRect adjustRect(QRect r);
    void onSelectRectChanged(int left, int top, int right ,int bottom);
    void onCursorPosChanged(int x, int y);

protected:
    virtual void contextMenuEvent(QContextMenuEvent *);
    virtual void mouseDoubleClickEvent(QMouseEvent*);
    virtual void mousePressEvent(QMouseEvent *);
    virtual void mouseReleaseEvent(QMouseEvent *e);
    virtual void mouseMoveEvent(QMouseEvent *e);
    virtual void keyPressEvent(QKeyEvent *e);
    virtual void paintEvent(QPaintEvent *);
    void updateMouse(bool force = false);

private:
    WindowManager* windowManager_ = nullptr;
    void handleArrowKeyPress(QKeyEvent *e);
    void initCursor();
    void initAmplifier(std::shared_ptr<QPixmap> originPainting = nullptr);
    void initMeasureWidget(void);
    void initDrawPanel(void);
    void recognizeSelection();
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
    void onLongScreenshotRequested(const QRect &captureRect);
    void onGifRecordingRequested(const QRect &captureRect);

private:
    Q_DISABLE_COPY(ScreenshotWidget)
    // 当前截图状态
    ScreenState currentState_;
    // 用于检测误操作
    QPoint startPoint_;
    // 屏幕暗色背景图
    std::shared_ptr<QPixmap> darkScreen_;
    // 屏幕原画
    std::shared_ptr<QPixmap> originScreen_;
    // 截图器大小感知器
    std::shared_ptr<SelectedScreenSizeWidget> sizeTextPanel_;
    // 放大取色器
    std::shared_ptr<AmplifierWidget> amplifierTool_;
    // 动作控制器
    std::shared_ptr<ScreenshotActionController> actionController_;
    // 鼠标光标位置感知区域
    QRect esthesiaRect_;

    // --- 探测器策略 ---
    std::unique_ptr<class IRectDetector> windowDetector_;
    std::unique_ptr<class IRectDetector> opencvDetector_;
    
    class IRectDetector* activeDetector() const;

    bool useOpenCVMode_;

    // 交互状态
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
    DIRECTION direction_ = DIR_NONE;
    bool isPressed_ = false;
    QPoint originPoint_;
    QPoint movePos_;
    QRect currentRect_;
    QPolygon listMarker_;
    QMenu *menu_ = nullptr;
    std::unique_ptr<class DrawPanel> drawPanel_;
    QTimer *mouseUpdateTimer_ = nullptr;
    QElapsedTimer mouseUpdateElapsed_;
    static constexpr int MOUSE_UPDATE_INTERVAL_MS = 16;
};
