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
#include <QProcess>
#include <thread>
#include "component/FramelessWidget.h"
#include "common/Constants.h"
#include "common/Util.h"
#include "controller/WindowManager.h"
#include "component/TipsWidget.h"
#include "view/DrawPanel.h"

static QList<QPair<QPoint, QPixmap>> HidedStickerList;
StickerWidget::StickerWidget(const QPixmap& pixmap, QWidget* parent)
    : QWidget(parent), pixmap_(pixmap)
{
	menu_ = new QMenu(this);
    menu_->addAction(QStringLiteral("±ê×¢"), this, SLOT(onDraw()), QKeySequence("Ctrl+D"));
    menu_->addAction(QStringLiteral("³·Ïú"), this, SLOT(onUndo()), QKeySequence("Ctrl+Z"));
	menu_->addAction(QStringLiteral("¸´ÖÆ"), this, SLOT(onCopy()), QKeySequence("Ctrl+C"));
    menu_->addAction(QStringLiteral("±£´æ"), this, SLOT(onSave()), QKeySequence("Ctrl+S"));
    if (!WindowManager::instance()->setting()->uploadImageUrl().isEmpty()) {
        menu_->addAction(QStringLiteral("ÉÏ´«Í¼´²"), this, SLOT(onUploadImg()));
    }
    menu_->addSeparator();
    menu_->addAction(QStringLiteral("Òþ²Ø"), this, SLOT(onHide()), QKeySequence("Ctrl+H"));
    menu_->addAction(QStringLiteral("Òþ²ØËùÓÐ"), this, SLOT(onHideAll()));
    menu_->addSeparator();
    menu_->addAction(QStringLiteral("Ïú»Ù"), this, SLOT(onClose()), QKeySequence("Esc"));
    menu_->addAction(QStringLiteral("Ïú»ÙËùÓÐ"), this, SLOT(onCloseAll()));

    this->setFocusPolicy(Qt::StrongFocus);
    emit WindowManager::instance()->sigStickerCountChanged();
}

StickerWidget::~StickerWidget()
{
    emit WindowManager::instance()->sigStickerCountChanged();
}

void StickerWidget::flush()
{
    if (drawPanel_) {
        drawPanel_->drawer()->drawPixmap(pixmap_);
    }
}

bool StickerWidget::hasBorder()
{
    return !WindowManager::instance()->setting()->pinNoBorder();
}

void StickerWidget::popup(const QPixmap &pixmap, const QPoint &pos)
{
    FramelessWidget* widget = new FramelessWidget();
	widget->setEnableHighlight(hasBorder());
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

void StickerWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && drawPanel_ && !drawPanel_->drawer()->isDraw()) {
        drawPanel_->hide();
    }
    QWidget::mousePressEvent(event);
}

void StickerWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (drawPanel_ && !drawPanel_->drawer()->isDraw()) {
        drawPanel_->onReferRectChanged(QRect(this->mapToGlobal(this->pos()), this->size()));
        drawPanel_->show();
    }
    QWidget::mouseReleaseEvent(event);
}

void StickerWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.drawPixmap(QPoint(0, 0), pixmap_, pixmap_.rect());
    if (drawPanel_) {
        drawPanel_->drawer()->onPaint(painter);
    }
}

void StickerWidget::onDraw()
{
    if (drawPanel_) {
        drawPanel_->drawer()->drawPixmap(pixmap_);
        drawPanel_.reset();
    } else {
        drawPanel_.reset(new DrawPanel(nullptr, this));
        connect(drawPanel_.get(), SIGNAL(sigSave()), this, SLOT(onSave()));
        connect(drawPanel_.get(), SIGNAL(sigFinished()), this, SLOT(onCopy()));
        drawPanel_->onReferRectChanged(QRect(this->mapToGlobal(this->pos()), this->size()));
        drawPanel_->show();
        drawPanel_->raise();
        drawPanel_->drawer()->setEnable(true);
    }
}

void StickerWidget::onUndo()
{
    if (drawPanel_) {
        drawPanel_->drawer()->undo();
    }
}

void StickerWidget::onCopy()
{
    flush();
	QClipboard* clipboard = QApplication::clipboard();
    clipboard->setPixmap(pixmap_);
    this->onClose();
}

void StickerWidget::onSave()
{
    flush();
    QString name = Util::pixmapName();
	QString fileName = QFileDialog::getSaveFileName(this, QStringLiteral("±£´æÍ¼Æ¬"), name, "PNG (*.png)");
	if (fileName.length() > 0) {
		pixmap_.save(fileName, "png");
	}
}

void StickerWidget::onUploadImg()
{
    QString writeDir = Util::getWritebaleDir();
    QString imgName = QString("ntscreenshot-") + QDateTime::currentDateTime().toString("hhmmss") + ".png";
    QString imgPath = writeDir + "/" + imgName;
    pixmap_.save(imgPath);

    QString command = QString("uploadimg.exe -url %1 -path %2").arg(WindowManager::instance()->setting()->uploadImageUrl()).arg(imgPath);
    QProcess process;
    process.setWorkingDirectory(Util::getRunDir());
    process.start(command);
    process.waitForFinished();
    QByteArray arr = process.readAllStandardOutput();
    QVariantMap vm = Util::json2map(arr);
    if (!vm["errcode"].isNull()) {
        qDebug() << vm;
        if (vm["errcode"].toInt() == 0) {
            QClipboard *clip = qApp->clipboard();
            clip->setText(vm["content"].toString());
        }
    } else {
        QString str = QString::fromLocal8Bit(arr);
        qDebug() << str;
    }
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
    QPoint pos = this->parentWidget()->pos();
    if (!pixmap_.isNull()) {
        flush();
        HidedStickerList.push_back(qMakePair(pos, pixmap_));
    }
    this->onClose();
}

void StickerWidget::onHideAll()
{
    hideAll();
}

//////////////////////////////////////////////////////////////////////////
//UploadImageUtil::UploadImageUtil(QWidget *parent)
//    : QObject(parent), parentWidget_(parent), http_(nullptr)
//{
//}
//
//void UploadImageUtil::upload(const QPixmap &pixmap)
//{
//    if (pixmap.isNull()) {
//        return;
//    }
//
//    if (!http_) {
//        http_ = new HttpRequest(this);
//        connect(http_, &HttpRequest::sigHttpResponse, this, &UploadImageUtil::onHttpResponse);
//    }
//
//    QByteArray b = Util::pixmap2ByteArray(pixmap);
//    if (b.isEmpty()) {
//        qDebug() << "upload image data is empty";
//        return;
//    }
//
//    QString fileName = Util::pixmapUniqueName(pixmap);
//    http_->postImage(WindowManager::instance()->setting()->uploadImageUrl(), fileName, b);
//}
//
//void UploadImageUtil::onHttpResponse(int err, const QByteArray& data)
//{
//    if (err != 0) {
//        QString tips = QStringLiteral("ÍøÂç´íÎó£¬´íÎóÂë£º%1").arg(err);
//        TipsWidget::popup(parentWidget_, tips, 5);
//    } else {
//        QVariantMap vm = Util::json2map(data);
//        if (vm["errcode"].toInt() == 0) {
//            TipsWidget::popup(parentWidget_, vm["url"].toString(), 5, 0, true);
//        } else {
//            QString tips = QStringLiteral("Ó¦´ð´íÎó£¬´íÎóÂë£º%1").arg(vm["errcode"].toInt());
//            TipsWidget::popup(parentWidget_, tips, 5, 0, true);
//        }
//    }
//}
