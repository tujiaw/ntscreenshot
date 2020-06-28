#pragma once
#include <QWidget>
#include <QMap>
#include <QColor>
#include <QPoint>
#include <QPen>
#include <QBrush>

class QPushButton;
class DrawMode
{
public:
    enum Shape{
        None = 0,
        PolyLine,
        Line,
        Arrow,
        Rectangle,
        Ellipse,
    };

    DrawMode();
    explicit DrawMode(Shape shape);

    void init();
    bool isNone() const;
    bool isValid() const;
    QPen& pen() { return pen_; }
    QBrush& brush() { return brush_; }
    void setPos(const QPoint &start, const QPoint &end);
    void addPos(const QPoint &pos);
    void clear();
    void draw(QPainter &painter);
    Shape shape() const { return shape_; }

    void initPainter(QPainter& painter);
    void drawPolyLine(const QVector<QPoint> &points, QPainter& painter);
    void drawLine(const QPoint& startPoint, const QPoint& endPoint, QPainter& painter);
    void drawArrows(const QPoint& startPoint, const QPoint& endPoint, QPainter &painter);
    void drawRect(const QPoint &startPoint, const QPoint &endPoint, QPainter& painter);
    void drawEllipse(const QPoint &startPoint, const QPoint &endPoint, QPainter& painter);

    bool operator==(const DrawMode& other) const;
    inline bool operator!=(const DrawMode& other) const { return !(operator==(other)); }

private:
    Shape shape_;
    QPoint start_;
    QPoint end_;
    QPen pen_;
    QBrush brush_;
    QVector<QPoint> points_;
};

//////////////////////////////////////////////////////////////////////////
class DrawPanel : public QWidget {
Q_OBJECT
public:
    DrawPanel(QWidget *parent);
    DrawMode getMode();
    void adjustPos();
    static QColor currentColor();

signals:
    void sigStart();
    void sigUndo();
    void sigSticker();
    void sigSave();
    void sigFinished();

public slots:
    void onReferRectChanged(const QRect &rect);
    void onShapeBtnClicked();
    void onColorBtnClicked();

protected:
    virtual void showEvent(QShowEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent* event);

private:
    QRect referRect_;
    QPushButton* selectedBtn_;
    QPushButton* pbColor_;
    QList<QPair<QPushButton*, DrawMode>> btns_;
};

