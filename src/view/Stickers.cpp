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

StickerWidget::StickerWidget(const QPixmap& pixmap, QWidget* parent)
    : QWidget(parent)
{
    uploadImageUtil_ = new UploadImageUtil(this);

	menu_ = new QMenu(this);
	menu_->addAction(QStringLiteral("¸´ÖÆ"), this, SLOT(onCopy()));
	menu_->addAction(QStringLiteral("±£´æ"), this, SLOT(onSave()));
    if (!WindowManager::instance()->setting()->uploadImageUrl().isEmpty()) {
        menu_->addAction(QStringLiteral("ÉÏ´«Í¼´²"), this, SLOT(onUpload()));
    }
	menu_->addAction(QStringLiteral("Ïú»Ù(Esc)"), this, SLOT(onClose()));
	menu_->addAction(QStringLiteral("Ïú»ÙËùÓÐ"), this, SLOT(onCloseAll()));

	label_ = new QLabel(this);
	label_->setPixmap(pixmap);

	QVBoxLayout* vLayout = new QVBoxLayout(this);
	vLayout->setContentsMargins(0, 0, 0, 0);
	vLayout->setSpacing(0);
	vLayout->addWidget(label_);
}

void StickerWidget::popup(const QPixmap &pixmap, const QPoint &pos)
{
    FramelessWidget* widget = new FramelessWidget();
    widget->setEnableEscClose(true);
	widget->setEnableHighlight(!WindowManager::instance()->setting()->pinNoBorder());
    StickerWidget* content = new StickerWidget(pixmap, widget);
    widget->setContent(content);
    widget->resize(pixmap.size());
    widget->move(pos);
    Util::setWndTopMost(widget);
    widget->show();
    widget->raise();
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
	QWidgetList widgets = QApplication::allWidgets();
	for (int i = 0; i < widgets.size(); i++) {
		QWidget* p = widgets.at(i);
		FramelessWidget* frame = qobject_cast<FramelessWidget*>(p);
		if (frame) {
			StickerWidget* sticker = qobject_cast<StickerWidget*>(frame->getContent());
			if (sticker) {
				frame->close();
			}
		}
	}
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
