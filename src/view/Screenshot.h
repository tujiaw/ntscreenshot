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
    // ���������Ƿ��Ѿ�չʾ
    bool isLeftPressed_;
    // ���ڼ�������
    QPoint startPoint_;
    // ��Ļ��ɫ����ͼ
    std::shared_ptr<QPixmap> darkScreen_;
    // ��Ļԭ��
    std::shared_ptr<QPixmap> originScreen_;
    // ��ͼ��Ļ
    std::shared_ptr<SelectedScreenWidget> selectedScreen_;
    // ��ͼ����С��֪��
    std::shared_ptr<SelectedScreenSizeWidget> sizeTextPanel_;
    // �Ŵ�ȡɫ��
    std::shared_ptr<AmplifierWidget> amplifierTool_;
    // �����λ�ø�֪����
    QRect esthesiaRect_;
};

//////////////////////////////////////////////////////////////////////////
// ��ʾѡ�������С
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
// ѡ������
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
    // ��λö��
    enum DIRECTION {
        DIR_NONE = 0,       // ��
        DIR_TOP = 1,        // ��
        DIR_BOTTOM,         // ��
        DIR_LEFT,           // ��
        DIR_RIGHT,          // ��
        DIR_LEFT_TOP,       // ����
        DIR_LEFT_BOTTOM,    // ����
        DIR_RIGHT_TOP,      // ����
        DIR_RIGHT_BOTTOM,   // ����
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
    // ���ڴ�С�ı�ʱ����¼�ı䷽��
    DIRECTION direction_;
    // ���
    QPoint originPoint_;
    // ����Ƿ���
    bool isPressed_;
    // �϶��ľ���
    QPoint movePos_;
    // ���ê��
    QPolygon listMarker_;
    // ��Ļԭ��
    std::shared_ptr<QPixmap> originScreen_;
    // ѡ�����򴰿�rect���ڻ��ƽ�ͼ����
    QRect currentRect_;
    // �Ҽ��˵�����
    QMenu *menu_;
    // ��ͼ���
    DrawPanel drawPanel_;
    // �ϴ�ͼƬ����
    class UploadImageUtil *uploadImageUtil_;
    // OCR
    class Ocr* ocr_;
};

