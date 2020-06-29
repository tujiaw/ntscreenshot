#pragma once

#include <memory>
#include <QWidget>
#include <QVariantMap>

class QLabel;
class QContextMenuEvent;
class QMenu;
class HttpRequest;
class DrawPanel;
class UploadImageUtil;

class StickerWidget : public QWidget
{
	Q_OBJECT
public:
	StickerWidget(const QPixmap& pixmap, QWidget* parent);
    ~StickerWidget();
    static void popup(const QPixmap &pixmap, const QPoint &pos);
    static void showAll();
    static void hideAll();
    static int allCount();
    static int visibleCount();
    static QList<StickerWidget*> getAllSticker();

protected:
    virtual void keyPressEvent(QKeyEvent *event);
	virtual void contextMenuEvent(QContextMenuEvent*);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void paintEvent(QPaintEvent *event);

private slots:
    void onDraw();
	void onCopy();
	void onSave();
    void onUpload();
	void onClose();
	void onCloseAll();
    void onHide();
    void onHideAll();

private:
    QPixmap pixmap_;
	QMenu* menu_;
    std::unique_ptr<DrawPanel> draw_;
    UploadImageUtil *uploadImageUtil_;
};

class UploadImageUtil : public QObject
{
    Q_OBJECT
public:
    UploadImageUtil(QWidget *parent);
    void upload(const QPixmap &pixmap);

private slots:
    void onHttpResponse(int err, const QByteArray& data);

private:
    QWidget *parentWidget_;
    HttpRequest* http_;
};
