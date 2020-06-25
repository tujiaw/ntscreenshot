#include "Stickers.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QMenu>
#include <QFileDialog>
#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QPainter>
#include <QImageReader>
#include <qmath.h>
#include <QSslConfiguration>
#include <qmessagebox.h>
#include "component/FramelessWidget.h"
#include "common/Constants.h"
#include "common/Util.h"
#include "common/HttpRequest.h"
#include "controller/WindowManager.h"
#include "component/TipsWidget.h"

static QList<QPair<QPoint, QPixmap>> HidedStickerList;
StickerWidget::StickerWidget(const QPixmap& pixmap, QWidget* parent)
    : QWidget(parent)
{
    uploadImageUtil_ = new UploadImageUtil(this);

	menu_ = new QMenu(this);
	menu_->addAction(QStringLiteral("¸´ÖÆ"), this, SLOT(onCopy()), QKeySequence("Ctrl+C"));
    menu_->addAction(QStringLiteral("±£´æ"), this, SLOT(onSave()), QKeySequence("Ctrl+S"));
    menu_->addSeparator();
    menu_->addAction(QStringLiteral("Òþ²Ø"), this, SLOT(onHide()), QKeySequence("Ctrl+H"));
    menu_->addAction(QStringLiteral("Òþ²ØËùÓÐ"), this, SLOT(onHideAll()));
    menu_->addSeparator();
    menu_->addAction(QStringLiteral("Ïú»Ù"), this, SLOT(onClose()), QKeySequence("Esc"));
    menu_->addAction(QStringLiteral("Ïú»ÙËùÓÐ"), this, SLOT(onCloseAll()));
    if (!WindowManager::instance()->setting()->uploadImageUrl().isEmpty()) {
        menu_->addSeparator();
        menu_->addAction(QStringLiteral("ÉÏ´«Í¼´²"), this, SLOT(onUpload()));
    }

	label_ = new QLabel(this);
	label_->setPixmap(pixmap);

	QVBoxLayout* vLayout = new QVBoxLayout(this);
	vLayout->setContentsMargins(0, 0, 0, 0);
	vLayout->setSpacing(0);
	vLayout->addWidget(label_);
    this->setFocusPolicy(Qt::StrongFocus);

    interval_handle_once(100, [this]() {
        emit WindowManager::instance()->sigStickerCountChanged();
    });
}

StickerWidget::~StickerWidget()
{
    interval_handle_once(100, [this]() {
        emit WindowManager::instance()->sigStickerCountChanged();
    });
}

void StickerWidget::popup(const QPixmap &pixmap, const QPoint &pos)
{
    FramelessWidget* widget = new FramelessWidget();
	widget->setEnableHighlight(!WindowManager::instance()->setting()->pinNoBorder());
    StickerWidget* content = new StickerWidget(pixmap, widget);
    widget->setContent(content);
    widget->resize(pixmap.size());
    widget->move(pos);
    Util::setWndTopMost(widget);
    widget->show();
    widget->raise();
}

void StickerWidget::showAll()
{
    qSort(HidedStickerList.begin(), HidedStickerList.end(), [](const QPair<QPoint, QPixmap> &left, const QPair<QPoint, QPixmap> &right)->bool {
        if (left.first.x() != right.first.x()) {
            return left.first.x() < right.first.x();
        }
        return left.first.y() < right.first.y();
    });
    for (int i = 0; i < HidedStickerList.size(); i++) {
        popup(HidedStickerList[i].second, HidedStickerList[i].first);
    }
    HidedStickerList.clear();
}

void StickerWidget::hideAll()
{
    QList<StickerWidget*> widgets = getAllSticker();
    for (int i = 0; i < widgets.size(); i++) {
        widgets[i]->onHide();
    }
}

int StickerWidget::allCount()
{
    return HidedStickerList.size() + visibleCount();
}

int StickerWidget::visibleCount()
{
    return getAllSticker().size();
}

QList<StickerWidget*> StickerWidget::getAllSticker()
{
    QList<StickerWidget*> result;
    QWidgetList widgets = QApplication::allWidgets();
    for (int i = 0; i < widgets.size(); i++) {
        QWidget* p = widgets.at(i);
        FramelessWidget* frame = qobject_cast<FramelessWidget*>(p);
        if (frame) {
            StickerWidget* sticker = qobject_cast<StickerWidget*>(frame->getContent());
            if (sticker) {
                result.push_back(sticker);
            }
        }
    }
    return result;
}

void StickerWidget::keyPressEvent(QKeyEvent *event)
{
    QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
    QString key = Util::strKeyEvent(keyEvent);
    if (!key.isEmpty()) {
        QList<QAction*> actions = menu_->actions();
        for (int i = 0; i < actions.size(); i++) {
            QKeySequence seq = actions[i]->shortcut();
            if (!seq.isEmpty() && Util::strKeySequence(seq) == key) {
                emit actions[i]->triggered();
                break;
            }
        }
    }
    QWidget::keyPressEvent(event);
}

void StickerWidget::contextMenuEvent(QContextMenuEvent*)
{
	menu_->exec(cursor().pos());
}

void StickerWidget::onCopy()
{
	QClipboard* clipboard = QApplication::clipboard();
	clipboard->setPixmap(*label_->pixmap());
}

void StickerWidget::onSave()
{
	QString fileName = QFileDialog::getSaveFileName(this, QStringLiteral("±£´æÍ¼Æ¬"), Util::screenshotDefaultName(), "JPEG Files (*.jpg)");
	if (fileName.length() > 0) {
		label_->pixmap()->save(fileName, "jpg");
	}
}

void StickerWidget::onUpload()
{
    uploadImageUtil_->upload(*label_->pixmap());
}

void StickerWidget::onClose()
{
	if (this->parentWidget()) {
		this->parentWidget()->close();
	}
}

void StickerWidget::onCloseAll()
{
    QList<StickerWidget*> widgets = getAllSticker();
    for (int i = 0; i < widgets.size(); i++) {
        widgets[i]->onClose();
    }
}

void StickerWidget::onHide()
{
    const QPixmap *pixmap = label_->pixmap();
    QPoint pos = this->parentWidget()->pos();
    if (pixmap && !pixmap->isNull()) {
        HidedStickerList.push_back(qMakePair(pos, *pixmap));
    }
    this->onClose();
}

void StickerWidget::onHideAll()
{
    hideAll();
}

//////////////////////////////////////////////////////////////////////////
UploadImageUtil::UploadImageUtil(QWidget *parent)
    : QObject(parent), parentWidget_(parent), http_(nullptr)
{
}

void UploadImageUtil::upload(const QPixmap &pixmap)
{
    if (pixmap.isNull()) {
        return;
    }

    if (!http_) {
        http_ = new HttpRequest(this);
        connect(http_, &HttpRequest::sigHttpResponse, this, &UploadImageUtil::onHttpResponse);
    }

    const QString FORMAT = "png";
    QByteArray b = Util::pixmap2ByteArray(pixmap, FORMAT.toStdString().c_str());
    if (b.isEmpty()) {
        qDebug() << "upload image data is empty";
        return;
    }

    QString fileName = Util::screenshotDefaultName() + "." + FORMAT;
    http_->postImage(WindowManager::instance()->setting()->uploadImageUrl(), fileName, b);
}

void UploadImageUtil::onHttpResponse(int err, const QByteArray& data)
{
    if (err != 0) {
        QString tips = QStringLiteral("ÍøÂç´íÎó£¬´íÎóÂë£º%1").arg(err);
        TipsWidget::popup(parentWidget_, tips, 5);
    } else {
        QVariantMap vm = Util::json2map(data);
        if (vm["errcode"].toInt() == 0) {
            TipsWidget::popup(parentWidget_, vm["url"].toString(), 5, 0, true);
        } else {
            QString tips = QStringLiteral("Ó¦´ð´íÎó£¬´íÎóÂë£º%1").arg(vm["errcode"].toInt());
            TipsWidget::popup(parentWidget_, tips, 5, 0, true);
        }
    }
}
