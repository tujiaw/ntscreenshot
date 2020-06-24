#pragma once

#include <QWidget>
#include <QVariantMap>

class QLabel;
class QContextMenuEvent;
class QMenu;
class HttpRequest;

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

private slots:
	void onCopy();
	void onSave();
    void onUpload();
	void onClose();
	void onCloseAll();
    void onHide();
    void onHideAll();

private:
	QLabel* label_;
	QMenu* menu_;
    class UploadImageUtil *uploadImageUtil_;
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
