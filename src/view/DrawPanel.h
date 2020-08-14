#pragma once
#include <QWidget>
#include <QMap>
#include <QColor>
#include <QPoint>
#include <QPen>
#include <QBrush>
#include <memory>
#include "DrawSettings.h"

class QPushButton;
class QAbstractButton;
class TextEdit;

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
    QFont& font() { return font_; }
    void setPos(const QPoint &start, const QPoint &end);
    void addPos(const QPoint &pos);
    void setText(const QRectF &rect, const QString& text);
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
    void drawText(const QRectF &rectangle, const QString& text, QPainter& painter);

private:
    Shape shape_;
    QPoint start_;
    QPoint end_;
    QPen pen_;
    QBrush brush_;
    QFont font_;
    QVector<QPoint> points_;
    QRectF textRect_;
    QString text_;
    QCursor cursor_;
};

//////////////////////////////////////////////////////////////////////////
class Drawer : public QObject {
    Q_OBJECT
public:
    Drawer(QWidget* parent);
    ~Drawer();
    QWidget* parentWidget();
    void setEnable(bool enable);
    bool enable() const;
    void setMode(const DrawMode &drawMode);
    const DrawMode& mode() const;
    bool isDraw() const;
    void undo();
    
    void drawPixmap(QPixmap &pixmap);
    void onPaint(QPainter &painter);
    void showTextEdit(const QPoint &pos);
    void saveText();

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
    TextEdit *textEdit_;
};

//////////////////////////////////////////////////////////////////////////
class DrawPanel : public QWidget {
Q_OBJECT
public:
    // 父窗口、需要绘制的窗口
    DrawPanel(QWidget *parent, QWidget *drawWidget);
    DrawMode getMode();
    void adjustPos();
    Drawer* drawer();
    static int fontSize();
    static QColor currentColor();

signals:
    void sigSticker();
    void sigSave();
    void sigFinished();

public slots:
    void onReferRectChanged(const QRect &rect);
    void onShapeBtnClicked(QAbstractButton*);
    void onColorBtnClicked();
    void onSettingChanged(int fontSize, QColor color);

protected:
    virtual void showEvent(QShowEvent *event);
    virtual void hideEvent(QHideEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent* event);

private:
    Q_DISABLE_COPY(DrawPanel)
    QRect referRect_;
    QPushButton *pbFont_;
    std::unique_ptr<DrawSettings> drawSettings_;
    QList<QPair<QPushButton*, DrawMode>> btns_;
    Drawer drawer_;
};

