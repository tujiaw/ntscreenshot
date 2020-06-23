#pragma once
#include <memory>
#include <QWidget>

class AmplifierWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AmplifierWidget(const std::shared_ptr<QPixmap> &originPainting, QWidget *parent = 0);
    QColor getCursorPointColor() const;

public slots:
    void onSizeChange(int w, int h);
    void onPostionChange(int x, int y);

protected:
    virtual void paintEvent(QPaintEvent *);

private:
    QSize screenSize_;
    QPoint cursorPoint_;
    int imageHeight_;
    QColor cursorPointColor_;
    std::shared_ptr<QPixmap> originPainting_;
};

