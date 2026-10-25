#pragma once

#include <QElapsedTimer>
#include <QTimer>
#include <QImage>
#include <QPointer>
#include <QWidget>
#include <QTransform>
#include "core/imaging/OcrResult.h"
#include "core/settings/SettingModel.h"

class PaddleOcrClient;

class OcrSelectionOverlay : public QWidget {
    Q_OBJECT
public:
    explicit OcrSelectionOverlay(QWidget* parent, bool fillParent = false);
    void recognize(const QImage& image, const PaddleOcrConfig& config);
    void reset();
    void leave();
    void setImageDisplayRect(const QRectF& rect);
    void setResult(const OcrResult& result, const QSize& imageSize);
    QString selectedText() const;
    bool handleKey(QKeyEvent* event);
    void copySelection();
    bool isRecognizing() const { return scanTimer_.isActive(); }

signals:
    void statusMessage(const QString& message, bool error);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;
    void wheelEvent(QWheelEvent*) override;

private:
    QRectF imageDisplayRect() const;
    QTransform imageTransform() const;
    QRectF imageDisplayRect_;
    QPointF imagePoint(const QPointF& point) const;
    int hitTest(const QPointF& point, bool nearest = false) const;
    void activate();
    void reportStatus(const QString& message, bool error = false);
    void showOverlay();
    QTimer scanTimer_;
    QElapsedTimer scanElapsed_;
    bool fillParent_ = false;
    OcrResult result_;
    QSize imageSize_;
    QImage source_;
    QPointer<PaddleOcrClient> client_;
    int anchor_ = -1;
    int extent_ = -1;
    bool dragging_ = false;
};
