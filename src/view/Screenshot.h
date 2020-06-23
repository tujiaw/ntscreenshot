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

    /// ���������Ƿ��Ѿ�չʾ
    bool isLeftPressed_;
    /// ���ڼ�������
    QPoint startPoint_;
    /// ��ǰ������Ļ�ľ�������
    QRect desktopRect_;
    /// ��Ļ��ɫ����ͼ
    std::shared_ptr<QPixmap> backgroundScreen_;
    /// ��Ļԭ��
    std::shared_ptr<QPixmap> originPainting_;
    /// ��ͼ��Ļ
    std::shared_ptr<SelectedScreenWidget> selectedScreen_;
    std::unique_ptr<DrawPanel> drawpanel_;
    /// ��ͼ����С��֪��
    std::shared_ptr<SelectedScreenSizeWidget> sizeTextPanel_;
    /// �Ŵ�ȡɫ��
    std::shared_ptr<AmplifierWidget> amplifierTool_;
    // �����λ�ø�֪����
    QRect esthesiaRect_;
    /// �ö���ʱ��
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
 * @brief : ��С��֪��
 * @note  : ��Ҫ�غ���ͼ�����Ϸ��Ĵ�С��֪�ؼ�
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
    /// �ڱ߾࣬������ק�Ĵ�����
    const int PADDING_ = 6;

    /// ��λö��
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
    /// ���ڴ�С�ı�ʱ����¼�ı䷽��
    DIRECTION       direction_;
    /// ���
    QPoint          originPoint_;
    QPoint          drawStartPos_, drawEndPos_;
    /// ����Ƿ���
    bool            isPressed_;
    /// �϶��ľ���
    QPoint          movePos_;
    /// ���ê��
    //QVector<QPoint> listMarker_;
    QPolygon listMarker_;
    /// ��Ļԭ��
    std::shared_ptr<QPixmap> originPainting_;
    /// ��ǰ���ڼ������� ���ڻ��ƽ�ͼ����
    QRect           currentRect_;
    /// �Ҽ��˵�����
    QMenu           *menu_;
    DrawMode drawMode_;
    QList<DrawMode> drawModeList_;
    bool isDrawMode_;
    class UploadImageUtil *uploadImageUtil_;
};

