#pragma once

#include <memory>
#include <QDir>
#include <QWidget>
#include <QVariantMap>

class QLabel;
class QContextMenuEvent;
class QMenu;
class DrawPanel;
class SettingModel;
class WindowManager;

class PinWidget : public QWidget
{
	Q_OBJECT
public:
	PinWidget(WindowManager* windowManager, const QPixmap& pixmap, QWidget* parent);
    ~PinWidget();
    void flush();
    QPixmap getPixmap() const;
    static bool hasBorder(const SettingModel* settings);
    static void popup(WindowManager* windowManager, const QPixmap &pixmap, const QPoint &pos);
    static void showAll(WindowManager* windowManager);
    static void hideAll();
    static int allCount();
    static int visibleCount();
    static QList<PinWidget*> getAllSticker();
    static QDir saveDir();
    static void setSaveDir(const QDir &dir);

protected:
    virtual void keyPressEvent(QKeyEvent *event);
	virtual void contextMenuEvent(QContextMenuEvent*);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void paintEvent(QPaintEvent *event);
    virtual void wheelEvent(QWheelEvent *event);

private slots:
    void onDraw();
    void onUndo();
	void onCopy();
	void onSave();
    void onUploadImg();
	void onClose();
	void onCloseAll();
    void onHide();
    void onHideAll();
    void hideScaleInfo();
    void onScanCode();
    void onOcr();
    void onSmartMask();
    void onAutoCrop();
    void onExtractColors();
    void onEnhance(int preset);

private:
    WindowManager* windowManager_ = nullptr;
    Q_DISABLE_COPY(PinWidget)
    void updateScaledPixmap(const QSize &targetSize);
    int getBorderWidth() const;
    void connectDrawPanelTools();
    void replacePixmap(const QPixmap& pixmap);
    QImage currentImage();
    void notifyToolMessage(const QString& message, bool isError = false);

    QPixmap originalPixmap_;
    QPixmap pixmap_;
	QMenu* menu_;
    std::unique_ptr<DrawPanel> drawPanel_;

    // 缩放比例显示
    double currentScale_ = 1.0;
    bool showScaleInfo_ = false;
};
