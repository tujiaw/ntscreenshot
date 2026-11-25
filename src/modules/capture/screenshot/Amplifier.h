#pragma once
#include <memory>
#include <QWidget>
#include "core/foundation/Constants.h"

class AmplifierWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AmplifierWidget(const std::shared_ptr<QPixmap> &originPainting, QWidget *parent = 0);
    QString getCursorPointColor() const;
    void setRgbColor(bool yes) { isRgbColor_ = yes; update(); }
    bool rgbColor() const { return isRgbColor_; }
    void setOpenCVMode(bool enabled) { useOpenCVMode_ = enabled; update(); }
    void setCrossColor(const QColor& color) { if (color.isValid()) { crossColor_ = color; update(); } }

public slots:
    void onSizeChanged(int w, int h);
    void onPositionChanged(int x, int y);

protected:
    virtual void paintEvent(QPaintEvent *);

private:
    Q_DISABLE_COPY(AmplifierWidget)
    void updateMetrics();
    QString modeText(bool smart) const;
    QPoint cursorPoint_;
    int imageHeight_ = 0;
    int rowHeight_ = 0;
    QColor cursorPointColor_;
    QColor crossColor_ = Style::AMPLIFIER_CROSS_COLOR;
    bool isRgbColor_ = false;
    bool useOpenCVMode_{false};
    std::shared_ptr<QPixmap> originPainting_;
    QImage originImage_;
};

