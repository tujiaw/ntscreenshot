#pragma once

#include "ImageDocument.h"
#include <QImage>
#include <QWidget>

namespace ImageBrowser {
class ImageCanvas final : public QWidget {
    Q_OBJECT
public:
    explicit ImageCanvas(QWidget* parent = nullptr);
    void setImage(const QImage& image, bool resetView = false);
    void setMessage(const QString& message);
    void fit(bool allowUpscale = true);
    void actualSize();
    void setZoom(double scale);
    void setLeftInset(int inset);
    double zoom() const { return scale_; }
    void setCropping(bool enabled);
    void setAnnotation(const EditOperation& operation, bool enabled);
    bool cancelDrawing();
    bool annotating() const { return annotating_; }
    bool cropping() const { return cropping_; }
    QRect cropRect() const;
    QPointF imagePoint(QPointF viewportPoint) const;
    QPointF viewportPoint(QPointF imagePoint) const;

signals:
    void annotationAccepted(const ImageBrowser::EditOperation& operation);
    void zoomChanged();
    void cropAccepted(const QRect& rect);
    void cropCancelled();
    void navigate(int delta);
    void viewportChanged();
    void selectionChanged();

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    void zoomAt(double scale, QPointF anchor);
    void previewMosaic(const QPolygonF& points);
    void updateDrawingCursor();
    QImage mosaicPreview_;
    QImage mosaicSource_;
    QPointF hoverPoint_;
    bool hovering_ = false;
    EditOperation annotation_, stroke_;
    bool annotating_ = false;
    bool drawing_ = false;
    QImage image_;
    QImage checkerboard_;
    QColor checkerColor_;
    QString message_;
    double scale_ = 1;
    QPointF offset_;
    bool fitted_ = true;
    bool fitUpscale_ = false;
    bool cropping_ = false;
    bool dragging_ = false;
    QPointF press_;
    QPointF initialOffset_;
    QRectF crop_;
    QRectF initialCrop_;
    int handle_ = 0;
    int leftInset_ = 0;
};
} // namespace ImageBrowser
