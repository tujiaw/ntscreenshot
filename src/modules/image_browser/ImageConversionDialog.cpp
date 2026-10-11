#include "ImageConversionDialog.h"
#include "ImageDocument.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QImageWriter>
#include <QLabel>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QSpinBox>

namespace ImageBrowser {
ImageConversionDialog::ImageConversionDialog(const QSize& imageSize, QWidget* parent)
    : QDialog(parent), originalSize_(imageSize) {
    setWindowTitle(tr("格式转换"));
    setMinimumWidth(380);
    form_ = new QFormLayout(this);
    formats_ = new QComboBox(this);
    formats_->setObjectName("conversionFormat");
    const auto supported = QImageWriter::supportedImageFormats();
    for (const QByteArray& format : {QByteArray("png"), QByteArray("jpeg"), QByteArray("bmp"), QByteArray("webp"), QByteArray("ico")})
        if (supported.contains(format)) formats_->addItem(QString::fromLatin1(format).toUpper(), format);
    formats_->addItem(QStringLiteral("PDF"), QByteArray("pdf"));
    form_->addRow(tr("目标格式"), formats_);
    resize_ = new QCheckBox(tr("调整输出尺寸"), this);
    resize_->setObjectName("conversionResize");
    form_->addRow(resize_);
    auto spin = [this](const char* name, int minimum, int maximum, int value) {
        auto* input = new QSpinBox(this);
        input->setObjectName(QString::fromLatin1(name));
        input->setRange(minimum, maximum); input->setValue(value);
        return input;
    };
    width_ = spin("conversionWidth", 1, MaxEditablePixels, imageSize.width());
    height_ = spin("conversionHeight", 1, MaxEditablePixels, imageSize.height());
    width_->setSuffix(tr(" 像素")); height_->setSuffix(tr(" 像素"));
    form_->addRow(tr("宽度"), width_); form_->addRow(tr("高度"), height_);
    lockRatio_ = new QCheckBox(tr("锁定宽高比"), this);
    lockRatio_->setChecked(true); form_->addRow(lockRatio_);
    connect(resize_, &QCheckBox::toggled, this, [this](bool enabled) {
        width_->setEnabled(enabled); height_->setEnabled(enabled); lockRatio_->setEnabled(enabled);
    });
    width_->setEnabled(false); height_->setEnabled(false); lockRatio_->setEnabled(false);
    auto syncSize = [this](bool fromWidth) {
        if (!lockRatio_->isChecked() || originalSize_.isEmpty()) return;
        QSpinBox* other = fromWidth ? height_ : width_;
        const QSignalBlocker blocker(other);
        const double ratio = double(originalSize_.width()) / originalSize_.height();
        other->setValue(qRound(qBound(1.0, fromWidth ? width_->value() / ratio : height_->value() * ratio, double(MaxEditablePixels))));
    };
    connect(width_, &QSpinBox::valueChanged, this, [syncSize] { syncSize(true); });
    connect(height_, &QSpinBox::valueChanged, this, [syncSize] { syncSize(false); });
    connect(lockRatio_, &QCheckBox::toggled, this, [syncSize](bool locked) { if (locked) syncSize(true); });
    color_ = new QComboBox(this); color_->setObjectName("conversionColor");
    color_->addItems({tr("彩色"), tr("灰度")}); form_->addRow(tr("色彩"), color_);
    icoSize_ = new QComboBox(this); icoSize_->setObjectName("conversionIcoSize");
    for (int size : {16, 24, 32, 48, 64, 128, 256})
        icoSize_->addItem(QStringLiteral("%1 × %1").arg(size), size);
    icoSize_->setCurrentIndex(icoSize_->count()-1); form_->addRow(tr("图标尺寸（像素）"), icoSize_);
    auto* icoHint = new QLabel(tr("等比缩放并居中，空白区域补透明；导出单尺寸 ICO 图标。"), this);
    icoHint->setObjectName("conversionIcoHint"); icoHint->setWordWrap(true); form_->addRow(icoHint);
    quality_ = spin("conversionQuality", 1, 100, 95); form_->addRow(tr("图片质量"), quality_);
    compression_ = spin("conversionCompression", 0, 100, 60);
    compression_->setToolTip(tr("数值越大，文件通常越小，保存越慢；不会损失画质。"));
    form_->addRow(tr("PNG 压缩程度"), compression_);
    paper_ = new QComboBox(this); paper_->setObjectName("conversionPaper");
    paper_->addItems({tr("按图片尺寸"), QStringLiteral("A4")}); form_->addRow(tr("PDF 页面"), paper_);
    orientation_ = new QComboBox(this); orientation_->setObjectName("conversionOrientation");
    orientation_->addItems({tr("自动"), tr("纵向"), tr("横向")}); form_->addRow(tr("页面方向"), orientation_);
    margin_ = spin("conversionMargin", 0, 50, 10); margin_->setSuffix(tr(" 毫米"));
    form_->addRow(tr("页边距"), margin_);
    auto* hint = new QLabel(tr("PDF 导出为单页，图片等比缩放居中；透明区域以白色显示。"), this);
    hint->setObjectName("conversionPdfHint"); hint->setWordWrap(true); form_->addRow(hint);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    form_->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (format() != "ico" && resize_->isChecked() && qint64(width_->value()) * height_->value() > MaxEditablePixels) {
            QMessageBox::warning(this, tr("格式转换"), tr("输出尺寸不能超过 7200 万像素。")); return;
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(formats_, &QComboBox::currentIndexChanged, this, &ImageConversionDialog::updateFormat);
    connect(paper_, &QComboBox::currentIndexChanged, this, &ImageConversionDialog::updateFormat);
    updateFormat();
}
QByteArray ImageConversionDialog::format() const { return formats_->currentData().toByteArray(); }
ImageConversionOptions ImageConversionDialog::options() const {
    ImageConversionOptions result;
    if (format() != "ico" && resize_->isChecked()) result.outputSize = QSize(width_->value(), height_->value());
    result.icoSize = icoSize_->currentData().toInt();
    result.quality = quality_->value(); result.pngCompression = compression_->value();
    result.grayscale = color_->currentIndex() == 1;
    result.pdfA4 = paper_->currentIndex() == 1;
    result.pdfOrientation = orientation_->currentIndex(); result.pdfMarginMm = margin_->value();
    return result;
}
void ImageConversionDialog::updateFormat() {
    const auto selected = format();
    const bool ico = selected == "ico";
    for (auto* field : {static_cast<QWidget*>(resize_), static_cast<QWidget*>(width_), static_cast<QWidget*>(height_), static_cast<QWidget*>(lockRatio_)})
        form_->setRowVisible(field, !ico);
    form_->setRowVisible(icoSize_, ico);
    form_->setRowVisible(findChild<QLabel*>("conversionIcoHint"), ico);
    form_->setRowVisible(quality_, selected == "jpeg" || selected == "webp");
    form_->setRowVisible(compression_, selected == "png");
    form_->setRowVisible(paper_, selected == "pdf");
    form_->setRowVisible(orientation_, selected == "pdf" && paper_->currentIndex() == 1);
    form_->setRowVisible(margin_, selected == "pdf" && paper_->currentIndex() == 1);
    form_->setRowVisible(findChild<QLabel*>("conversionPdfHint"), selected == "pdf");
}
}
