#include "ImageEditorPanel.h"
#include "EditorValueSpinBox.h"
#include "core/theme/ThemeManager.h"
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLineF>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace ImageBrowser {
namespace {
QIcon editorToolIcon(int tool, const QColor& color) {
    QPixmap pixmap(64, 64); pixmap.fill(Qt::transparent);
    QPainter p(&pixmap); p.scale(2, 2); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (tool == 0) {
        p.drawLine(9, 5, 9, 23); p.drawLine(9, 23, 27, 23);
        p.drawLine(5, 9, 23, 9); p.drawLine(23, 9, 23, 27);
        p.drawArc(QRectF(15, 2, 14, 14), 15*16, 95*16); p.drawLine(28, 6, 28, 12);
    } else if (tool == 1) {
        QPainterPath path; path.moveTo(7, 23); path.lineTo(10, 16); path.lineTo(23, 4);
        path.lineTo(28, 9); path.lineTo(15, 22); path.closeSubpath(); p.drawPath(path);
        p.drawLine(18, 26, 27, 26); p.drawLine(7, 23, 4, 27); p.drawLine(4, 27, 12, 25);
    } else if (tool == 2) {
        QPolygonF grid; grid << QPointF(6, 6) << QPointF(26, 3) << QPointF(26, 25) << QPointF(6, 29);
        p.drawPolygon(grid); p.drawLine(12, 5, 12, 28); p.drawLine(19, 4, 19, 26);
        p.drawLine(6, 13, 26, 10); p.drawLine(6, 21, 26, 18);
    } else if (tool == 3) {
        p.drawRoundedRect(QRectF(5, 5, 22, 22), 3, 3);
        for (int y = 0; y < 3; ++y) for (int x = 0; x < 3; ++x)
            if ((x+y)%2 == 0) p.fillRect(QRectF(7+x*6, 7+y*6, 6, 6), color);
    } else if (tool == 4) {
        QPainterPath drop; drop.moveTo(15, 3); drop.cubicTo(8, 11, 4, 16, 7, 22);
        drop.cubicTo(10, 29, 20, 29, 23, 22); drop.cubicTo(26, 16, 22, 10, 15, 3);
        p.drawPath(drop); p.drawLine(16, 19, 28, 15); p.drawLine(17, 24, 29, 20);
    } else if (tool == 5) {
        p.drawRoundedRect(QRectF(3, 4, 18, 18), 5, 5); p.drawRoundedRect(QRectF(12, 13, 17, 17), 3, 3);
    } else if (tool == 6) {
        p.drawEllipse(QRectF(10, 3, 13, 13)); p.drawEllipse(QRectF(3, 14, 13, 13)); p.drawEllipse(QRectF(17, 14, 13, 13));
    } else if (tool == 7) {
        p.drawEllipse(QRectF(3, 4, 26, 24));
        p.setBrush(color);
        for (const QPointF& point : {QPointF(10, 11), QPointF(16, 8), QPointF(23, 12), QPointF(9, 18)}) p.drawEllipse(point, 1.2, 1.2);
        p.setBrush(Qt::NoBrush); p.drawEllipse(QRectF(17, 19, 8, 8));
    } else {
        p.drawEllipse(QRectF(3, 19, 7, 7)); p.drawEllipse(QRectF(12, 22, 7, 7));
        p.drawLine(8, 20, 24, 4); p.drawLine(15, 23, 7, 5);
        p.setPen(QPen(color, 1.5, Qt::DashLine)); p.drawRect(QRectF(17, 11, 11, 15));
    }
    return QIcon(pixmap);
}

class DetailStack final : public QStackedWidget {
public:
    using QStackedWidget::QStackedWidget;
    QSize sizeHint() const override { return currentWidget() ? currentWidget()->sizeHint() : QSize(); }
    QSize minimumSizeHint() const override { return currentWidget() ? currentWidget()->minimumSizeHint() : QSize(); }
};
}

QIcon ImageEditorPanel::editIcon() { return editorToolIcon(1, Qt::white); }

ImageEditorPanel::ImageEditorPanel(QWidget* parent) : QFrame(parent) {
    setObjectName(QStringLiteral("browserEditorPanel"));
    setAutoFillBackground(true);
    auto* layout = new QVBoxLayout(this);
    auto* header = new QHBoxLayout;
    header->addWidget(new QLabel(tr("图片编辑"), this));
    header->addStretch();
    auto* close = new QToolButton(this);
    close->setObjectName("editorClose");
    close->setText(QStringLiteral("×")); close->setToolTip(tr("关闭编辑面板"));
    connect(close, &QToolButton::clicked, this, &ImageEditorPanel::closeRequested);
    header->addWidget(close); layout->addLayout(header);

    tools_ = new QButtonGroup(this);
    tools_->setExclusive(true);
    toolGrid_ = new QWidget(this);
    auto* grid = new QGridLayout(toolGrid_);
    grid->setContentsMargins(0, 0, 0, 0);
    const QStringList names = {tr("裁剪旋转"), tr("标注"), tr("矫正"), tr("马赛克"),
                               tr("水印"), tr("素材"), tr("滤镜"), tr("调色"), tr("抠图")};
    for (int i = 0; i < names.size(); ++i) {
        auto* button = new QToolButton(toolGrid_);
        button->setObjectName(QStringLiteral("browserEditorTool%1").arg(i));
        button->setProperty("editorTool", i);
        button->setText(names[i]); button->setToolTip(names[i]);
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setCheckable(true); button->setCursor(Qt::PointingHandCursor);
        tools_->addButton(button, i);
        grid->addWidget(button, i / 4, i % 4);
    }
    layout->addWidget(toolGrid_);
    auto* divider = new QFrame(this); divider->setFrameShape(QFrame::HLine);
    layout->addWidget(divider);
    scroll_ = new QScrollArea(this);
    scroll_->setObjectName(QStringLiteral("browserEditorDetails"));
    scroll_->setWidgetResizable(true); scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_->setMinimumHeight(0);
    pages_ = new DetailStack(scroll_);
    scroll_->setWidget(pages_); layout->addWidget(scroll_, 1);

    auto* crop = createPage(names[0], tr("拖动选区裁剪，或直接旋转、翻转和调整尺寸。"));
    auto* transforms = new QWidget(crop); auto* transformGrid = new QGridLayout(transforms);
    transformGrid->setContentsMargins(0, 0, 0, 0);
    const QStringList transformNames = {tr("向左旋转"), tr("向右旋转"), tr("水平翻转"), tr("垂直翻转")};
    for (int i = 0; i < 4; ++i) {
        auto* button = new QPushButton(transformNames[i], transforms);
        button->setObjectName(QStringLiteral("editorTransform%1").arg(i));
        transformGrid->addWidget(button, i / 2, i % 2);
        connect(button, &QPushButton::clicked, this, [this, i] {
            EditOperation operation; operation.kind = static_cast<EditOperation::Kind>(EditOperation::RotateLeft + i);
            emit editRequested(operation, false);
        });
    }
    qobject_cast<QFormLayout*>(crop->layout())->addRow(transforms);
    resizeWidth_ = addSpin(crop, tr("宽度（像素）"), "editorResizeWidth", 1, int(MaxEditablePixels), 1);
    resizeHeight_ = addSpin(crop, tr("高度（像素）"), "editorResizeHeight", 1, int(MaxEditablePixels), 1);
    aspectLock_ = new QCheckBox(tr("锁定宽高比"), crop); aspectLock_->setChecked(true);
    qobject_cast<QFormLayout*>(crop->layout())->addRow(aspectLock_);
    connect(resizeWidth_, &QSpinBox::valueChanged, this, [this](int width) {
        if (aspectLock_->isChecked() && imageSize_.isValid()) {
            const QSignalBlocker blocker(resizeHeight_);
            resizeHeight_->setValue(qMax(1, qRound(double(width) * imageSize_.height() / imageSize_.width())));
        }
    });
    connect(resizeHeight_, &QSpinBox::valueChanged, this, [this](int height) {
        if (aspectLock_->isChecked() && imageSize_.isValid()) {
            const QSignalBlocker blocker(resizeWidth_);
            resizeWidth_->setValue(qMax(1, qRound(double(height) * imageSize_.width() / imageSize_.height())));
        }
    });
    auto* resize = new QPushButton(tr("应用尺寸"), crop); resize->setObjectName("editorResizeApply");
    qobject_cast<QFormLayout*>(crop->layout())->addRow(resize);
    connect(resize, &QPushButton::clicked, this, [this] {
        const QSize size(resizeWidth_->value(), resizeHeight_->value());
        if (qint64(size.width()) * size.height() > MaxEditablePixels) {
            showMessage(tr("图片尺寸超过 7200 万像素的编辑上限。")); return;
        }
        if (size == imageSize_) return;
        EditOperation operation; operation.kind = EditOperation::Resize; operation.size = size;
        emit editRequested(operation, false);
    });

    auto* annotate = createPage(names[1], tr("选择图形后直接拖动绘制，松开完成；文字点击放置，Esc 取消当前绘制。"));
    auto* annotationForm = qobject_cast<QFormLayout*>(annotate->layout());
    annotationShape_ = new QComboBox(annotate); annotationShape_->setObjectName("editorAnnotationShape");
    annotationShape_->addItems({tr("矩形"), tr("椭圆"), tr("箭头"), tr("文字")});
    annotationForm->addRow(tr("形状"), annotationShape_);
    annotationText_ = new QLineEdit(annotate); annotationText_->setObjectName("editorAnnotationText");
    annotationText_->setPlaceholderText(tr("输入标注文字")); annotationForm->addRow(tr("文字"), annotationText_);
    annotationColor_ = addColor(annotate, "editorAnnotationColor");
    annotationWidth_ = addSpin(annotate, tr("线宽（像素）"), "editorAnnotationWidth", 1, 100, 4);
    annotationFont_ = addSpin(annotate, tr("字号（像素）"), "editorAnnotationFont", 8, 200, 28);
    auto updateAnnotation = [this, annotationForm] {
        const bool text = annotationShape_->currentIndex() == 3;
        annotationForm->setRowVisible(annotationText_, text);
        annotationForm->setRowVisible(annotationFont_, text);
        annotationForm->setRowVisible(annotationWidth_, !text);
        pages_->updateGeometry();
    };
    connect(annotationShape_, &QComboBox::currentIndexChanged, this, updateAnnotation); updateAnnotation();

    auto* perspective = createPage(names[2], tr("按左上、右上、右下、左下顺序输入原图坐标。"));
    const QStringList cornerNames = {tr("左上"), tr("右上"), tr("右下"), tr("左下")};
    for (int i = 0; i < 4; ++i) {
        auto* row = new QWidget(perspective); auto* fields = new QHBoxLayout(row);
        fields->setContentsMargins(0, 0, 0, 0);
        for (int axis = 0; axis < 2; ++axis) {
            fields->addWidget(new QLabel(axis ? "Y" : "X", row));
            auto* coordinate = new QSpinBox(row); coordinate->setRange(0, 0);
            coordinate->setObjectName(QStringLiteral("editorCorner%1").arg(i*2+axis));
            fields->addWidget(coordinate, 1); corners_ << coordinate;
        }
        qobject_cast<QFormLayout*>(perspective->layout())->addRow(cornerNames[i], row);
    }
    auto* mosaic = createPage(names[3], tr("选中需要遮挡的区域，块越大，马赛克越明显。"));
    mosaicBlock_ = addSpin(mosaic, tr("块大小（像素）"), "editorMosaicBlock", 2, 100, 12);
    auto* watermark = createPage(names[4], tr("选择常用水印布局，或自定义放置区域。"));
    watermarkText_ = new QLineEdit(watermark); watermarkText_->setObjectName("editorWatermarkText");
    watermarkText_->setPlaceholderText(tr("输入水印文字"));
    qobject_cast<QFormLayout*>(watermark->layout())->addRow(tr("文字"), watermarkText_);
    watermarkMode_ = new QComboBox(watermark); watermarkMode_->setObjectName("editorWatermarkMode");
    watermarkMode_->addItems({tr("自定义区域"), tr("右下角"), tr("左下角"), tr("右上角"), tr("左上角"), tr("居中"), tr("平铺")});
    watermarkMode_->setCurrentIndex(1);
    qobject_cast<QFormLayout*>(watermark->layout())->addRow(tr("布局"), watermarkMode_);
    watermarkColor_ = addColor(watermark, "editorWatermarkColor");
    watermarkFont_ = addSpin(watermark, tr("字号（像素）"), "editorWatermarkFont", 8, 200, 36);
    watermarkOpacity_ = addSpin(watermark, tr("不透明度（%）"), "editorWatermarkOpacity", 0, 100, 45);
    watermarkMargin_ = addSpin(watermark, tr("边距（像素）"), "editorWatermarkMargin", 0, 1000, 24);
    auto* sticker = createPage(names[5], tr("导入本地图片作为素材，再选择放置区域。"));
    assetPath_ = new QLineEdit(sticker); assetPath_->setObjectName("editorAssetPath");
    assetPath_->setPlaceholderText(tr("素材图片路径"));
    qobject_cast<QFormLayout*>(sticker->layout())->addRow(assetPath_);
    auto* browse = new QPushButton(tr("选择素材图片"), sticker);
    qobject_cast<QFormLayout*>(sticker->layout())->addRow(browse);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, tr("选择素材图片"), imagePath_,
            tr("图片 (*.png *.jpg *.jpeg *.bmp *.webp)"));
        if (!path.isEmpty()) assetPath_->setText(path);
    });
    auto* filters = createPage(names[6], tr("选择滤镜，点击应用后生效。"));
    filter_ = new QComboBox(filters); filter_->setObjectName("editorFilter");
    filter_->addItems({tr("黑白"), tr("复古"), tr("反色"), tr("高斯模糊"), tr("中值降噪"), tr("锐化"),
        tr("边缘检测"), tr("自动二值化"), tr("自动对比度")});
    qobject_cast<QFormLayout*>(filters->layout())->addRow(tr("滤镜"), filter_);
    filterStrength_ = addSpin(filters, tr("强度"), "editorFilterStrength", 1, 20, 3);
    auto* filterHint = new QLabel(filters); filterHint->setWordWrap(true);
    qobject_cast<QFormLayout*>(filters->layout())->addRow(filterHint);
    auto updateFilter = [this, filters, filterHint] {
        const int mode = filter_->currentIndex();
        const bool parameter = mode >= 3 && mode <= 6;
        auto* form = qobject_cast<QFormLayout*>(filters->layout());
        form->setRowVisible(filterStrength_, parameter);
        filterStrength_->setMaximum(mode == 6 ? 255 : mode == 4 ? 4 : 20);
        filterStrength_->setValue(mode == 6 ? 80 : mode == 5 ? 10 : 3);
        qobject_cast<QLabel*>(form->labelForField(filterStrength_))->setText(mode == 6 ? tr("边缘阈值") : mode == 3 || mode == 4 ? tr("半径（像素）") : tr("强度"));
        const QStringList hints = {tr("转为灰度，保留透明度。"), tr("使用复古色彩效果。"), tr("反转颜色，保留透明度。"),
            tr("平滑图像，半径越大越模糊。"), tr("去除椒盐噪点，适合少量孤立杂点。"), tr("增强边缘细节，强度过高可能出现光晕。"),
            tr("输出黑底白色轮廓，阈值越高保留的边缘越少。"), tr("自动计算阈值，输出黑白图像。"), tr("拉伸明暗范围，改善偏灰图像。")};
        filterHint->setText(hints[mode]); pages_->updateGeometry();
    };
    connect(filter_, &QComboBox::currentIndexChanged, this, updateFilter); updateFilter();
    auto* color = createPage(names[7], tr("调整参数后点击应用，可使用撤销恢复。"));
    auto colorControl = [this, color](const QString& label, const QString& name, const QString& gradient, bool alpha = false) {
        auto* row = new QWidget(color); auto* layout = new QVBoxLayout(row); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(2);
        auto* header = new QHBoxLayout;
        auto* spin = new EditorValueSpinBox(row); spin->setRange(alpha ? 0 : -100, 100); spin->setObjectName(name); spin->setFixedWidth(78); spin->setFixedHeight(26);
        header->addWidget(new QLabel(label, row)); header->addStretch(); header->addWidget(spin); layout->addLayout(header);
        auto* slider = new QSlider(Qt::Horizontal, row); slider->setRange(alpha ? 0 : -100, 100);
        slider->setObjectName(name + "Slider"); slider->setFixedHeight(16);
        slider->setStyleSheet(QStringLiteral("QSlider::groove:horizontal { height: 8px; border-radius: 2px; background: qlineargradient(x1:0,y1:0,x2:1,y2:0,%1); } QSlider::sub-page:horizontal { background:transparent; border:0; } QSlider::handle:horizontal { background:white; width:8px; margin:-4px 0; border-radius:1px; }").arg(gradient));
        layout->addWidget(slider);
        connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
        connect(spin, &QSpinBox::valueChanged, slider, &QSlider::setValue);
        spin->setValue(alpha ? 100 : 0);
        qobject_cast<QFormLayout*>(color->layout())->addRow(row); return spin;
    };
    brightness_ = colorControl(tr("亮度"), "editorBrightness", "stop:0 #252525, stop:1 #eeeeee");
    contrast_ = colorControl(tr("对比度"), "editorContrast", "stop:0 #777777, stop:0.5 #eeeeee, stop:1 #252525");
    saturation_ = colorControl(tr("饱和度"), "editorSaturation", "stop:0 #888888, stop:0.5 #53be72, stop:1 #eb533c");
    temperature_ = colorControl(tr("色温"), "editorTemperature", "stop:0 #397bd5, stop:1 #d8a72c");
    tint_ = colorControl(tr("色调"), "editorTint", "stop:0 #30c85a, stop:1 #b84dc9");
    opacity_ = colorControl(tr("不透明度"), "editorOpacity", "stop:0 #555555, stop:1 #ffffff", true);
    auto* reset = new QPushButton(tr("重置参数"), color); reset->setObjectName("editorColorReset");
    qobject_cast<QFormLayout*>(color->layout())->addRow(reset);
    connect(reset, &QPushButton::clicked, this, [this] {
        for (auto* field : {brightness_, contrast_, saturation_, temperature_, tint_}) field->setValue(0);
        opacity_->setValue(100);
    });

    auto* cutout = createPage(names[8], tr("框选完整主体，选区外保留背景，再应用选区去除背景。适合主体与背景较清晰的图片；复杂发丝和透明物体可能需要重新选区。"));
    cutoutIterations_ = addSpin(cutout, tr("细化次数"), "editorCutoutIterations", 1, 5, 3);
    cutoutFeather_ = addSpin(cutout, tr("边缘羽化（像素）"), "editorCutoutFeather", 0, 10, 1);
    auto* cutoutHint = new QLabel(tr("保留原图尺寸和透明度，建议另存为 PNG。增加细化次数会延长处理时间。"), cutout);
    cutoutHint->setWordWrap(true); qobject_cast<QFormLayout*>(cutout->layout())->addRow(cutoutHint);

    message_ = new QLabel(this); message_->setObjectName("editorMessage"); message_->setWordWrap(true);
    message_->setTextFormat(Qt::PlainText); layout->addWidget(message_);
    submit_ = new QPushButton(this); submit_->setObjectName("editorSubmit"); layout->addWidget(submit_);
    auto* selectionRow = new QHBoxLayout;
    applySelection_ = new QPushButton(tr("应用选区"), this); applySelection_->setObjectName("editorApplySelection");
    cancelSelection_ = new QPushButton(tr("取消选区"), this); cancelSelection_->setObjectName("editorCancelSelection");
    selectionRow->addWidget(cancelSelection_); selectionRow->addWidget(applySelection_); layout->addLayout(selectionRow);
    connect(submit_, &QPushButton::clicked, this, &ImageEditorPanel::submit);
    connect(applySelection_, &QPushButton::clicked, this, [this] {
        if (validateOperation(currentOperation())) emit applySelectionRequested();
    });
    connect(cancelSelection_, &QPushButton::clicked, this, &ImageEditorPanel::cancelSelectionRequested);
    connect(tools_, &QButtonGroup::idClicked, this, &ImageEditorPanel::selectTool);
    for (auto* field : {annotationWidth_, annotationFont_})
        connect(field, &QSpinBox::valueChanged, this, &ImageEditorPanel::drawingStyleChanged);
    for (auto* field : {annotationColor_, annotationText_})
        connect(field, &QLineEdit::textChanged, this, &ImageEditorPanel::drawingStyleChanged);
    connect(annotationShape_, &QComboBox::currentIndexChanged, this, &ImageEditorPanel::drawingStyleChanged);
    connect(watermarkMode_, &QComboBox::currentIndexChanged, this, [this] {
        if (selecting_) emit cancelSelectionRequested();
        syncButtons();
    });
    tools_->button(0)->setChecked(true); selectTool(0);
}

QWidget* ImageEditorPanel::createPage(const QString& title, const QString& description) {
    auto* page = new QWidget(pages_);
    auto* form = new QFormLayout(page); form->setContentsMargins(0, 0, 0, 0);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    auto* heading = new QLabel(title, page); auto font = heading->font(); font.setBold(true); heading->setFont(font);
    form->addRow(heading);
    auto* hint = new QLabel(description, page); hint->setWordWrap(true); form->addRow(hint);
    pages_->addWidget(page); return page;
}

QSpinBox* ImageEditorPanel::addSpin(QWidget* page, const QString& label, const QString& name,
                                  int minimum, int maximum, int value) {
    auto* field = new QSpinBox(page); field->setObjectName(name); field->setRange(minimum, maximum); field->setValue(value);
    qobject_cast<QFormLayout*>(page->layout())->addRow(label, field); return field;
}

QLineEdit* ImageEditorPanel::addColor(QWidget* page, const QString& name) {
    auto* container = new QWidget(page); auto* layout = new QVBoxLayout(container); layout->setContentsMargins(0, 0, 0, 0);
    auto* field = new QLineEdit(QStringLiteral("#ff0000"), container); field->setObjectName(name);
    field->setMaxLength(9); field->setPlaceholderText("#RRGGBB"); layout->addWidget(field);
    auto* swatches = new QHBoxLayout;
    for (const QString& value : {QStringLiteral("#ff0000"), QStringLiteral("#ffcc00"), QStringLiteral("#00b86b"),
                                  QStringLiteral("#3284ff"), QStringLiteral("#ffffff"), QStringLiteral("#000000")}) {
        auto* button = new QToolButton(container); button->setProperty("colorSwatch", true);
        button->setToolTip(value); button->setStyleSheet(QStringLiteral("QToolButton { background: %1; border: 1px solid #888; }").arg(value));
        swatches->addWidget(button); connect(button, &QToolButton::clicked, field, [field, value] { field->setText(value); });
    }
    layout->addLayout(swatches); qobject_cast<QFormLayout*>(page->layout())->addRow(tr("颜色"), container); return field;
}

void ImageEditorPanel::selectTool(int tool) {
    const bool changed = currentTool_ != tool;
    currentTool_ = tool; pages_->setCurrentIndex(tool); pages_->updateGeometry();
    scroll_->verticalScrollBar()->setValue(0); showMessage({}); syncButtons();
    if (changed) emit toolChanged();
    emit drawingStyleChanged();
}

EditOperation ImageEditorPanel::currentOperation() const {
    EditOperation operation;
    if (currentTool_ == 0) return operation;
    operation.kind = static_cast<EditOperation::Kind>(EditOperation::Annotate + currentTool_ - 1);
    switch (currentTool_) {
    case 1:
        operation.variant = annotationShape_->currentIndex(); operation.text = annotationText_->text();
        operation.color = QColor(annotationColor_->text());
        operation.strength = operation.variant == 3 ? annotationFont_->value() : annotationWidth_->value(); break;
    case 2: {
        for (int i = 0; i < 8; i += 2) operation.corners << QPointF(corners_[i]->value(), corners_[i+1]->value());
        const auto& p = operation.corners;
        operation.size = QSize(qRound(qMax(QLineF(p[0], p[1]).length(), QLineF(p[3], p[2]).length())) + 1,
                               qRound(qMax(QLineF(p[0], p[3]).length(), QLineF(p[1], p[2]).length())) + 1); break;
    }
    case 3: operation.strength = mosaicBlock_->value(); break;
    case 4:
        operation.text = watermarkText_->text(); operation.color = QColor(watermarkColor_->text());
        operation.strength = watermarkFont_->value(); operation.variant = watermarkMode_->currentIndex();
        operation.opacity = watermarkOpacity_->value(); operation.margin = watermarkMargin_->value(); break;
    case 5: operation.asset = ImageDocument::read(assetPath_->text()).image; break;
    case 6: operation.variant = filter_->currentIndex(); operation.strength = filterStrength_->value(); break;
    case 7: operation.brightness = brightness_->value(); operation.contrast = contrast_->value(); operation.saturation = saturation_->value();
        operation.temperature = temperature_->value(); operation.tint = tint_->value(); operation.opacity = opacity_->value(); break;
    case 8: operation.strength = cutoutIterations_->value(); operation.feather = cutoutFeather_->value(); break;
    }
    return operation;
}

void ImageEditorPanel::submit() {
    if (!editable_) return;
    const auto operation = currentOperation();
    if (!validateOperation(operation)) return;
    if (currentTool_ == 7 && operation.brightness == 0 && operation.contrast == 0 && operation.saturation == 0 && operation.temperature == 0 && operation.tint == 0 && operation.opacity == 100) return;
    showMessage({});
    emit editRequested(operation, currentTool_ == 0 || currentTool_ == 3 || (currentTool_ == 4 && watermarkMode_->currentIndex() == 0) || currentTool_ == 5 || currentTool_ == 8);
}

bool ImageEditorPanel::validateOperation(const EditOperation& operation) {
    if (!operation.color.isValid()) { showMessage(tr("请输入有效颜色，例如 #ff0000。")); return false; }
    if ((currentTool_ == 4 || (currentTool_ == 1 && operation.variant == 3)) && operation.text.trimmed().isEmpty()) {
        showMessage(tr("请先输入文字。")); return false;
    }
    if (currentTool_ == 5 && operation.asset.isNull()) { showMessage(tr("请选择可读取的素材图片。")); return false; }
    return true;
}

void ImageEditorPanel::setDocumentContext(const QString& path, QSize size, bool editable) {
    editable_ = editable;
    toolGrid_->setEnabled(editable); pages_->setEnabled(editable);
    if (imageSize_ != size || imagePath_ != path) {
        imageSize_ = size; imagePath_ = path;
        const QSignalBlocker widthBlocker(resizeWidth_), heightBlocker(resizeHeight_);
        resizeWidth_->setValue(qMax(1, size.width())); resizeHeight_->setValue(qMax(1, size.height()));
        for (int i = 0; i < 4; ++i) {
            corners_[i*2]->setMaximum(qMax(0, size.width()-1));
            corners_[i*2+1]->setMaximum(qMax(0, size.height()-1));
            corners_[i*2]->setValue((i == 1 || i == 2) ? qMax(0, size.width()-1) : 0);
            corners_[i*2+1]->setValue(i >= 2 ? qMax(0, size.height()-1) : 0);
        }
        showMessage({});
    }
    syncButtons();
}

void ImageEditorPanel::setSelectionState(bool selecting, const QRect& rect) {
    selecting_ = selecting; validSelection_ = !rect.isEmpty();
    syncButtons();
    if (selecting) showMessage(rect.isEmpty() ? tr("在图片上拖动选择区域。")
        : tr("选区 %1 × %2，按 Enter 或点击应用选区。").arg(rect.width()).arg(rect.height()));
    else showMessage({});
}

void ImageEditorPanel::syncButtons() {
    const bool region = currentTool_ == 0 || currentTool_ == 3 || (currentTool_ == 4 && watermarkMode_->currentIndex() == 0) || currentTool_ == 5 || currentTool_ == 8;
    submit_->setText(region ? (selecting_ ? tr("重新选择区域") : tr("选择区域")) : tr("应用"));
    submit_->setVisible(currentTool_ != 1);
    submit_->setEnabled(editable_);
    applySelection_->setVisible(selecting_); cancelSelection_->setVisible(selecting_);
    applySelection_->setEnabled(editable_ && validSelection_); cancelSelection_->setEnabled(editable_);
}

void ImageEditorPanel::showMessage(const QString& text) { message_->setText(text); message_->setVisible(!text.isEmpty()); }

void ImageEditorPanel::setMetrics(double scale, int availableHeight) {
    const auto& theme = ThemeManager::tokens();
    const QString key = theme.surface.name() + theme.border.name() + theme.surfaceRaised.name()
        + theme.textPrimary.name() + theme.accentSubtle.name() + theme.accent.name() + QString::number(currentTool_);
    if (lastScale_ == scale && lastHeight_ == availableHeight && lastStyleKey_ == key) return;
    lastScale_ = scale; lastHeight_ = availableHeight; lastStyleKey_ = key;
    setStyleSheet(QStringLiteral(
        "QFrame#browserEditorPanel { background: %1; border: 1px solid %2; border-radius: 10px; }"
        "#browserEditorPanel QScrollArea, #browserEditorPanel QStackedWidget, #browserEditorPanel QScrollArea QWidget { background: %1; }"
        "#browserEditorPanel QToolButton { background: %3; color: %4; border: 0; border-radius: 6px; padding: 4px; }"
        "#browserEditorPanel QToolButton:hover, #browserEditorPanel QToolButton:checked { background: %5; }"
        "#browserEditorPanel QPushButton#editorSubmit { background: %6; color: white; border-radius: 6px; padding: %7px; }")
        .arg(theme.surface.name(), theme.border.name(), theme.surfaceRaised.name(), theme.textPrimary.name(),
             theme.accentSubtle.name(), theme.accent.name()).arg(qRound(6*scale)));
    auto* outer = qobject_cast<QVBoxLayout*>(layout());
    outer->setContentsMargins(qRound(10*scale), qRound(8*scale), qRound(10*scale), qRound(8*scale));
    outer->setSpacing(qRound(6*scale));
    const bool compact = availableHeight < qRound(660*scale);
    findChild<QToolButton*>(QStringLiteral("editorClose"))->setFixedSize(qRound(24*scale), qRound(24*scale));
    for (auto* abstract : tools_->buttons()) {
        auto* button = qobject_cast<QToolButton*>(abstract);
        button->setFixedSize(qRound(72*scale), qRound((compact ? 52 : 64)*scale));
        button->setIconSize(QSize(qRound((compact ? 22 : 25)*scale), qRound((compact ? 22 : 25)*scale)));
        button->setIcon(editorToolIcon(tools_->id(button), theme.textPrimary));
    }
    for (auto* swatch : findChildren<QToolButton*>())
        if (swatch->property("colorSwatch").toBool()) swatch->setFixedSize(qRound(22*scale), qRound(22*scale));
    for (auto* field : {brightness_, contrast_, saturation_, temperature_, tint_, opacity_}) {
        static_cast<EditorValueSpinBox*>(field)->setControlScale(scale);
        if (auto* slider = field->parentWidget()->findChild<QSlider*>()) slider->setFixedHeight(qRound(16*scale));
    }
    setFixedWidth(qRound(344*scale));
    setFixedHeight(qMin(qRound((currentTool_ == 7 || currentTool_ == 8 || currentTool_ == 6 ? 740 : 620)*scale), availableHeight));
}
} // namespace ImageBrowser
