#pragma once
#include <memory>
#include <QWidget>

class AmplifierWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AmplifierWidget(const std::shared_ptr<QPixmap> &originPainting, QWidget *parent = 0);
    QString getCursorPointColor() const;
    void setRgbColor(bool yes) { isRgbColor_ = yes; }
    bool rgbColor() const { return isRgbColor_; }

public slots:
    void onSizeChanged(int w, int h);
    void onPositionChanged(int x, int y);

protected:
    virtual void paintEvent(QPaintEvent *);

private:
    Q_DISABLE_COPY(AmplifierWidget)
    QSize screenSize_;
    QPoint cursorPoint_;
    int imageHeight_;
    QColor cursorPointColor_;
    bool isRgbColor_;
    std::shared_ptr<QPixmap> originPainting_;
};

