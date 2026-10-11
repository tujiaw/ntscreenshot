#pragma once

#include "ImageConversionOptions.h"
#include <QDialog>

class QComboBox;
class QSpinBox;
class QCheckBox;
class QFormLayout;

namespace ImageBrowser {
class ImageConversionDialog final : public QDialog {
    Q_OBJECT
public:
    ImageConversionDialog(const QSize& imageSize, QWidget* parent = nullptr);
    QByteArray format() const;
    ImageConversionOptions options() const;
private:
    void updateFormat();
    QSize originalSize_;
    QFormLayout* form_;
    QComboBox* formats_;
    QCheckBox* resize_;
    QCheckBox* lockRatio_;
    QSpinBox* width_;
    QSpinBox* height_;
    QComboBox* color_;
    QSpinBox* quality_;
    QSpinBox* compression_;
    QComboBox* paper_;
    QComboBox* orientation_;
    QSpinBox* margin_;
};
}
