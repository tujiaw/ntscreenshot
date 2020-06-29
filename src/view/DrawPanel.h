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
    enum Shape {
        None = 0,
        PolyLine,
        Line,
        Arrow,
        Rectangle,
        Ellipse,
        Text,
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
    void setText(const QString& text);
    void clear();
    void draw(QPainter &painter);
    Shape shape() const { return shape_; }
    const QCursor& cursor() const { return cursor_; }

    void initPainter(QPainter& painter);
    void drawPolyLine(const QVector<QPoint> &points, QPainter& painter);
    void drawLine(const QPoint& startPoint, const QPoint& endPoint, QPainter& painter);
    void drawArrows(const QPoint& startPoint, const QPoint& endPoint, QPainter &painter);
    void drawRect(const QPoint &startPoint, const QPoint &endPoint, QPainter& painter);
    void drawEllipse(const QPoint &startPoint, const QPoint &endPoint, QPainter& painter);
    void drawText(const QPoint& startPoint, const QString& text, QPainter& painter);

private:
    Shape shape_;
    QPoint start_;
    QPoint end_;
    QPen pen_;
    QBrush brush_;
    QVector<QPoint> points_;
    QString text_;
    QCursor cursor_;
};

//////////////////////////////////////////////////////////////////////////
class Drawer : public QObject {
    Q_OBJECT
public:
    Drawer(QWidget* parent);
    void setEnable(bool enable);
    bool enable() const;
    void setDrawMode(const DrawMode &drawMode);
    const DrawMode& drawMode() const;
    void drawUndo();
    
    void drawPixmap(QPixmap &pixmap);
    void onPaint(QPainter &painter);

protected:
    bool eventFilter(QObject* watched, QEvent* event);
    bool onMousePressEvent(QMouseEvent *e);
    bool onMouseReleaseEvent(QMouseEvent *e);
    bool onMouseMoveEvent(QMouseEvent *e);

private:
    QWidget* parent_;
    bool isEnabled_;
    // 绘制开始位置
    QPoint drawStartPos_;
    // 绘制结束位置
    QPoint drawEndPos_;
    // 鼠标是否按下
    bool isPressed_;
    // 绘制模式
    DrawMode drawMode_;
    // 历史绘制模式缓存
    QList<DrawMode> drawModeCache_;
};

//////////////////////////////////////////////////////////////////////////
class DrawPanel : public QWidget {
Q_OBJECT
public:
    DrawPanel(QWidget *widget, QWidget *drawWidget);
    DrawMode getMode();
    void adjustPos();
    Drawer* drawer();
    static QColor currentColor();

signals:
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
    Drawer drawer_;
};

