#include "PinWidget.h"
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
#include <QDateTime>
#include <qmessagebox.h>
#include <QProcess>
#include <QStandardPaths>
#include <QWheelEvent>
#include <QTimer>
#include <thread>
#include <algorithm>
#include "shared/ui/FramelessWidget.h"
#include "core/foundation/Constants.h"
#include "core/platform/Util.h"
#include "app/WindowManager.h"
#include "shared/ui/TipsWidget.h"
#include "modules/capture/annotation/DrawPanel.h"
#include "core/settings/SettingModel.h"
#include "core/theme/ThemeIcon.h"
#include "core/imaging/CodeScanner.h"
#include "core/imaging/ImageEnhance.h"
#include "core/imaging/ImageUtil.h"
#include "core/imaging/SmartMask.h"
#include "core/network/PaddleOcrClient.h"
#include "shared/ui/TipsWidget.h"

static QList<QPair<QPoint, QPixmap>> HidedStickerList;
static QDir SaveDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);

PinWidget::PinWidget(WindowManager* windowManager, const QPixmap& pixmap, QWidget* parent)
    : QWidget(parent), windowManager_(windowManager), originalPixmap_(pixmap), pixmap_(pixmap)
{
    menu_ = new QMenu(this);

    menu_->addAction(QStringLiteral("标注"), this, SLOT(onDraw()), QKeySequence("Ctrl+D"));
    menu_->addAction(QStringLiteral("撤销"), this, SLOT(onUndo()), QKeySequence("Ctrl+Z"));
	menu_->addAction(QStringLiteral("复制"), this, SLOT(onCopy()), QKeySequence("Ctrl+C"));
    menu_->addAction(QStringLiteral("保存"), this, SLOT(onSave()), QKeySequence("Ctrl+S"));
    if (windowManager_->setting()->paddleOcrConfig().enabled) {
        menu_->addAction(QStringLiteral("OCR 识别"), this, SLOT(onOcr()));
    }
    {
        auto *s = windowManager_->setting();
        if (!s->gitHubImageBedConfig().token.isEmpty()) {
            menu_->addAction(QStringLiteral("上传图床"), this, SLOT(onUploadImg()));
        }
    }
    menu_->addSeparator();
    QMenu* imageToolsMenu = menu_->addMenu(ThemeIcon::icon("mosaic.png"), QStringLiteral("马赛克 / 图像工具"));
    imageToolsMenu->addAction(QStringLiteral("识别二维码/条码"), this, SLOT(onScanCode()));
    QMenu* enhanceMenu = imageToolsMenu->addMenu(QStringLiteral("图像增强"));
    enhanceMenu->addAction(QStringLiteral("自动增强"), this, [this]() { onEnhance(0); });
    enhanceMenu->addAction(QStringLiteral("提亮"), this, [this]() { onEnhance(1); });
    enhanceMenu->addAction(QStringLiteral("对比度"), this, [this]() { onEnhance(2); });
    enhanceMenu->addAction(QStringLiteral("锐化"), this, [this]() { onEnhance(3); });
    enhanceMenu->addAction(QStringLiteral("降噪"), this, [this]() { onEnhance(4); });
    imageToolsMenu->addAction(QStringLiteral("智能打码"), this, SLOT(onSmartMask()));
    imageToolsMenu->addAction(QStringLiteral("自动裁边"), this, SLOT(onAutoCrop()));
    imageToolsMenu->addAction(QStringLiteral("提取主色"), this, SLOT(onExtractColors()));
    menu_->addSeparator();
    menu_->addAction(QStringLiteral("隐藏"), this, SLOT(onHide()), QKeySequence("Ctrl+H"));
    menu_->addAction(QStringLiteral("隐藏所有"), this, SLOT(onHideAll()));
    menu_->addSeparator();
    menu_->addAction(QStringLiteral("销毁"), this, SLOT(onClose()), QKeySequence("Esc"));
    menu_->addAction(QStringLiteral("销毁所有"), this, SLOT(onCloseAll()));

    this->setFocusPolicy(Qt::StrongFocus);
    emit windowManager_->sigStickerCountChanged();
}

PinWidget::~PinWidget()
{
    emit windowManager_->sigStickerCountChanged();
}

void PinWidget::flush()
{
    if (drawPanel_) {
        drawPanel_->drawer()->drawPixmap(pixmap_);
    }
}

QPixmap PinWidget::getPixmap() const
{
    return pixmap_;
}

bool PinWidget::hasBorder(const SettingModel* settings)
{
    return settings && !settings->pinNoBorder();
}

void PinWidget::popup(WindowManager* windowManager, const QPixmap &pixmap, const QPoint &pos)
{
    FramelessWidget* widget = new FramelessWidget();
	widget->setEnableHighlight(hasBorder(windowManager->setting()));
    PinWidget* content = new PinWidget(windowManager, pixmap, widget);
    widget->setContent(content);
    widget->resize(pixmap.size());
    widget->move(pos);
    Util::setWndTopMost(widget);
    widget->show();
    widget->raise();
}

void PinWidget::showAll(WindowManager* windowManager)
{
    std::sort(HidedStickerList.begin(), HidedStickerList.end(), [](const QPair<QPoint, QPixmap> &left, const QPair<QPoint, QPixmap> &right) -> bool {
        if (left.first.x() != right.first.x()) {
            return left.first.x() < right.first.x();
        }
        return left.first.y() < right.first.y();
    });
    for (int i = 0; i < HidedStickerList.size(); i++) {
        popup(windowManager, HidedStickerList[i].second, HidedStickerList[i].first);
    }
    HidedStickerList.clear();
}

void PinWidget::hideAll()
{
    QList<PinWidget*> widgets = getAllSticker();
    for (int i = 0; i < widgets.size(); i++) {
        widgets[i]->onHide();
    }
}

int PinWidget::allCount()
{
    return HidedStickerList.size() + visibleCount();
}

int PinWidget::visibleCount()
{
    return getAllSticker().size();
}

QList<PinWidget*> PinWidget::getAllSticker()
{
    QList<PinWidget*> result;
    QWidgetList widgets = QApplication::allWidgets();
    for (int i = 0; i < widgets.size(); i++) {
        QWidget* p = widgets.at(i);
        FramelessWidget* frame = qobject_cast<FramelessWidget*>(p);
        if (frame) {
            PinWidget* sticker = qobject_cast<PinWidget*>(frame->getContent());
            if (sticker) {
                result.push_back(sticker);
            }
        }
    }
    return result;
}

QDir PinWidget::saveDir()
{
    return SaveDir;
}

void PinWidget::setSaveDir(const QDir &dir)
{
    SaveDir = dir;
}

void PinWidget::keyPressEvent(QKeyEvent *event)
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

void PinWidget::contextMenuEvent(QContextMenuEvent*)
{
	menu_->exec(cursor().pos());
}

void PinWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && drawPanel_ && !drawPanel_->drawer()->isDraw()) {
        drawPanel_->hide();
    }
    QWidget::mousePressEvent(event);
}

void PinWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (drawPanel_ && drawPanel_->drawer() && drawPanel_->drawer()->isDraw()) {
        this->setCursor(drawPanel_->drawer()->mode().cursor());
    } else {
        this->setCursor(Qt::ArrowCursor);
    }
    QWidget::mouseMoveEvent(event);
}

void PinWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (drawPanel_ && !drawPanel_->drawer()->isDraw()) {
        drawPanel_->onReferRectChanged(QRect(this->mapToGlobal(QPoint(0, 0)), this->size()));
        drawPanel_->show();
    }
    QWidget::mouseReleaseEvent(event);
}

void PinWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.drawPixmap(QPoint(0, 0), pixmap_, pixmap_.rect());
    if (drawPanel_) {
        drawPanel_->drawer()->onPaint(painter);
    }

    // 绘制缩放比例信息
    if (showScaleInfo_) {
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QString scaleText = QString::number(static_cast<int>(currentScale_ * 100)) + "%";
        
        QFont font("Arial");
        font.setPixelSize(18);
        font.setBold(true);
        painter.setFont(font);
        const QFontMetrics fm(font);
        const QRect textRect = fm.boundingRect(scaleText);
        
        const int padding = 8;
        const QRect bgRect(10, 10, textRect.width() + padding * 2, textRect.height() + padding * 2);
        
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 160));
        painter.drawRoundedRect(bgRect, 4, 4);
        
        painter.setPen(QColor(255, 255, 255));
        painter.drawText(bgRect, Qt::AlignCenter, scaleText);
    }
}

void PinWidget::wheelEvent(QWheelEvent *event)
{
    if (!this->parentWidget()) {
        QWidget::wheelEvent(event);
        return;
    }

    const int delta = event->angleDelta().y();
    if (delta == 0) {
        QWidget::wheelEvent(event);
        return;
    }

    const double scaleFactor = delta > 0 ? 1.1 : 0.9;
    QWidget* parent = this->parentWidget();
    const QSize oldSize = parent->size();
    
    int newWidth = static_cast<int>(oldSize.width() * scaleFactor);
    int newHeight = static_cast<int>(oldSize.height() * scaleFactor);
    
    const int borderWidth = getBorderWidth();
    const int originalWidth = originalPixmap_.width();
    const int originalHeight = originalPixmap_.height();
    const int targetOriginalWidth = originalWidth + borderWidth * 2;
    const int targetOriginalHeight = originalHeight + borderWidth * 2;

    if (newWidth < 20 || newHeight < 20) {
        event->accept();
        return;
    }

    if (drawPanel_ && drawPanel_->isVisible()) {
        drawPanel_->hide();
    }
    
    const double scaleToOriginal = static_cast<double>(newWidth) / targetOriginalWidth;
    
    bool isOriginal = false;
    if (scaleToOriginal > 0.95 && scaleToOriginal < 1.05) {
        newWidth = targetOriginalWidth;
        newHeight = targetOriginalHeight;
        isOriginal = true;
    }
    
    const QSize newSize(newWidth, newHeight);
    parent->resize(newSize);
    
    const QSize contentSize = QSize(
        (newSize.width() - borderWidth * 2),
        (newSize.height() - borderWidth * 2)
    );
    
    if (isOriginal) {
        pixmap_ = originalPixmap_;
    }
    else {
        pixmap_ = originalPixmap_.scaled(contentSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    
    currentScale_ = static_cast<double>(pixmap_.width()) / originalPixmap_.width();
    showScaleInfo_ = true;
    update();
    
    QTimer::singleShot(3000, this, &PinWidget::hideScaleInfo);
    
    event->accept();
}

int PinWidget::getBorderWidth() const
{
    FramelessWidget* frame = qobject_cast<FramelessWidget*>(this->parentWidget());
    if (frame && frame->enableHightlight()) {
        return 1;
    }
    return 0;
}

void PinWidget::updateScaledPixmap(const QSize &targetSize)
{
    if (originalPixmap_.isNull()) {
        return;
    }
    
    if (targetSize == originalPixmap_.size()) {
        pixmap_ = originalPixmap_;
    } else {
        pixmap_ = originalPixmap_.scaled(targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
}

void PinWidget::connectDrawPanelTools()
{
    if (!drawPanel_) {
        return;
    }
    connect(drawPanel_.get(), &DrawPanel::sigScanCode, this, &PinWidget::onScanCode);
    connect(drawPanel_.get(), &DrawPanel::sigOcr, this, &PinWidget::onOcr);
    connect(drawPanel_.get(), &DrawPanel::sigSmartMask, this, &PinWidget::onSmartMask);
    connect(drawPanel_.get(), &DrawPanel::sigAutoCrop, this, &PinWidget::onAutoCrop);
    connect(drawPanel_.get(), &DrawPanel::sigExtractColors, this, &PinWidget::onExtractColors);
    connect(drawPanel_.get(), &DrawPanel::sigEnhance, this, &PinWidget::onEnhance);
}

QImage PinWidget::currentImage()
{
    QPixmap pix = pixmap_;
    if (drawPanel_) {
        drawPanel_->drawer()->drawPixmap(pix);
    }
    return pix.toImage();
}

void PinWidget::replacePixmap(const QPixmap& pixmap)
{
    if (pixmap.isNull()) {
        return;
    }
    if (!drawPanel_) onDraw();
    const QPixmap previousPixmap = pixmap_;
    const QPixmap previousOriginal = originalPixmap_;
    const double previousScale = currentScale_;
    drawPanel_->drawer()->clearForTransformation(
        [this, previousPixmap, previousOriginal, previousScale]() {
            pixmap_ = previousPixmap;
            originalPixmap_ = previousOriginal;
            currentScale_ = previousScale;
            if (QWidget* parent = parentWidget()) {
                const int border = getBorderWidth();
                parent->resize(pixmap_.size() + QSize(border * 2, border * 2));
            }
            drawPanel_->onReferRectChanged(QRect(mapToGlobal(QPoint(0, 0)), size()));
            update();
        });
    originalPixmap_ = pixmap;
    pixmap_ = pixmap;
    currentScale_ = 1.0;
    if (QWidget* parent = parentWidget()) {
        const int border = getBorderWidth();
        parent->resize(pixmap.size() + QSize(border * 2, border * 2));
    }
    if (drawPanel_) {
        drawPanel_->onReferRectChanged(QRect(mapToGlobal(QPoint(0, 0)), size()));
    }
    update();
}

void PinWidget::notifyToolMessage(const QString& message, bool isError)
{
    // Ensure the annotation toolbar exists so messages stay under it.
    if (!drawPanel_) {
        onDraw();
    }
    if (drawPanel_) {
        drawPanel_->showToolMessage(message, isError);
        return;
    }
    TipsWidget::popup(this, message, isError ? 3 : 4, 0, !isError);
}

void PinWidget::onScanCode()
{
    const QVector<CodeScanResult> results = CodeScanner::scan(currentImage());
    if (results.isEmpty()) {
        notifyToolMessage(QStringLiteral("未识别到二维码/条码"), true);
        return;
    }

    QStringList parts;
    for (const CodeScanResult& item : results) {
        if (item.type.isEmpty() || item.type == QStringLiteral("QR")) {
            parts.push_back(item.text);
        } else {
            parts.push_back(QStringLiteral("[%1] %2").arg(item.type, item.text));
        }
    }
    const QString text = parts.join(QLatin1Char('\n'));
    QApplication::clipboard()->setText(text);
    const QString title = results.size() > 1
        ? QStringLiteral("识别成功（%1 条，已复制）").arg(results.size())
        : QStringLiteral("识别成功（已复制）");
    notifyToolMessage(title + QLatin1Char('\n') + text, false);
}

void PinWidget::onOcr()
{
    const PaddleOcrConfig config = windowManager_->setting()->paddleOcrConfig();
    if (!config.enabled) {
        notifyToolMessage(QStringLiteral("请先在设置 / 图片中启用 OCR"), true);
        return;
    }

    const QImage image = currentImage();
    if (image.isNull()) {
        notifyToolMessage(QStringLiteral("OCR 失败：贴图数据为空"), true);
        return;
    }

    notifyToolMessage(QStringLiteral("OCR 识别中…"), false);
    auto* client = new PaddleOcrClient(this);
    connect(client, &PaddleOcrClient::succeeded, this, [this, client](const QString& text) {
        QApplication::clipboard()->setText(text);
        notifyToolMessage(QStringLiteral("OCR 识别成功，文本已复制到剪切板"), false);
        client->deleteLater();
    });
    connect(client, &PaddleOcrClient::failed, this, [this, client](const QString& error) {
        notifyToolMessage(QStringLiteral("OCR 失败：%1").arg(error), true);
        client->deleteLater();
    });
    client->recognize(Util::pixmap2ByteArray(QPixmap::fromImage(image)), config);
}

void PinWidget::onSmartMask()
{
    QImage src = currentImage();
    if (SmartMask::detectRegions(src).isEmpty()) {
        notifyToolMessage(QStringLiteral("未检测到可打码区域"), true);
        return;
    }
    QImage out = SmartMask::autoMask(src);
    if (out.isNull() || out.size() != src.size()) {
        notifyToolMessage(QStringLiteral("未检测到可打码区域"), true);
        return;
    }
    replacePixmap(QPixmap::fromImage(out));
    notifyToolMessage(QStringLiteral("已智能打码"), false);
}

void PinWidget::onAutoCrop()
{
    QImage src = currentImage();
    QImage cropped = ImageUtil::AutoCropAdaptive(src);
    if (cropped.isNull() || cropped.size() == src.size()) {
        notifyToolMessage(QStringLiteral("无需裁边"), true);
        return;
    }
    replacePixmap(QPixmap::fromImage(cropped));
    notifyToolMessage(QStringLiteral("已自动裁边"), false);
}

void PinWidget::onExtractColors()
{
    const QVector<QColor> colors = ImageUtil::DominantColors(currentImage(), 5);
    if (colors.isEmpty()) {
        notifyToolMessage(QStringLiteral("未能提取主色"), true);
        return;
    }
    QStringList hexes;
    for (const QColor& c : colors) {
        hexes.push_back(c.name(QColor::HexRgb).toUpper());
    }
    const QString text = hexes.join(QLatin1Char(' '));
    QApplication::clipboard()->setText(text);
    notifyToolMessage(QStringLiteral("主色已复制") + QLatin1Char('\n') + text, false);
}

void PinWidget::onEnhance(int preset)
{
    QImage out = ImageEnhance::apply(currentImage(), static_cast<ImageEnhance::Preset>(preset));
    if (out.isNull()) {
        notifyToolMessage(QStringLiteral("图像增强失败"), true);
        return;
    }
    replacePixmap(QPixmap::fromImage(out));
    notifyToolMessage(QStringLiteral("已应用图像增强"), false);
}

void PinWidget::onDraw()
{
    if (drawPanel_) {
        drawPanel_->drawer()->drawPixmap(pixmap_);
        drawPanel_.reset();
    } else {
        drawPanel_.reset(new DrawPanel(windowManager_->setting(), nullptr, this));
        connect(drawPanel_.get(), &DrawPanel::sigSave, this, &PinWidget::onSave);
        connect(drawPanel_.get(), &DrawPanel::sigFinished, this, &PinWidget::onCopy);
        connectDrawPanelTools();
        drawPanel_->onReferRectChanged(QRect(this->mapToGlobal(QPoint(0, 0)), this->size()));
        drawPanel_->show();
        drawPanel_->raise();
        drawPanel_->drawer()->setEnable(true);
    }
}

void PinWidget::onUndo()
{
    if (drawPanel_) {
        drawPanel_->drawer()->undo();
    }
}

void PinWidget::onCopy()
{
    flush();
	QClipboard* clipboard = QApplication::clipboard();
    clipboard->setPixmap(pixmap_);
    this->onClose();
}

void PinWidget::onSave()
{
    flush();
    QString name = Util::pixmapName();
    QString savePath = PinWidget::saveDir().absoluteFilePath(name);
    
    QPixmap p = pixmap_;
    this->onClose();
    
    QTimer::singleShot(0, [p, savePath]() {
        QString fileName = QFileDialog::getSaveFileName(nullptr, QStringLiteral("保存图片"), savePath, "PNG (*.png)");
        if (fileName.length() > 0) {
            PinWidget::setSaveDir(QFileInfo(fileName).absoluteDir());
            p.save(fileName, "png");
        }
    });
}

void PinWidget::onUploadImg()
{
    QString imgName = QString("ntscreenshot-%1.png").arg(QDateTime::currentDateTime().toString("hhmmss"));
    auto *setting = windowManager_->setting();

    const GitHubImageBedConfig ghConfig = setting->gitHubImageBedConfig();
    if (ghConfig.token.isEmpty()) {
        return;
    }

    flush();
    QPixmap p = pixmap_;
    this->onClose();

    std::thread([p, imgName, ghConfig]() {
        Util::UploadResult result = Util::uploadToGitHub(Util::pixmap2ByteArray(p), imgName, ghConfig);
        QMetaObject::invokeMethod(qApp, [result]() {
            if (result.success) {
                QApplication::clipboard()->setText(result.fullUrl);
            } else {
                QMessageBox::warning(nullptr, QStringLiteral("上传图床失败"), result.message);
            }
        }, Qt::QueuedConnection);
    }).detach();
}

void PinWidget::onClose()
{
	if (this->parentWidget()) {
		this->parentWidget()->close();
	}
}

void PinWidget::onCloseAll()
{
    QList<PinWidget*> widgets = getAllSticker();
    for (int i = 0; i < widgets.size(); i++) {
        widgets[i]->onClose();
    }
}

void PinWidget::onHide()
{
    QPoint pos = this->parentWidget()->pos();
    if (!originalPixmap_.isNull()) {
        flush();
        HidedStickerList.push_back(qMakePair(pos, pixmap_));
    }
    this->onClose();
}

void PinWidget::onHideAll()
{
    hideAll();
}

void PinWidget::hideScaleInfo()
{
    showScaleInfo_ = false;
    update();
}
