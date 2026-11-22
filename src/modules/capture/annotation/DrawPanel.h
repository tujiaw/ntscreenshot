#pragma once
#include <QWidget>
#include <QMap>
#include <QColor>
#include <QPoint>
#include <QPen>
#include <QBrush>
#include <QImage>
#include <functional>
#include <memory>
#include "DrawSettings.h"
#include "Stamp.h"

class QPushButton;
class QAbstractButton;
class QLabel;
class QTextEdit;
class QTimer;
class TextEdit;
class QButtonGroup;
class QHideEvent;
class QHBoxLayout;
class QMouseEvent;
class QPainter;
class QShowEvent;

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
        Mosaic,
        Bitmap,
        Stamp,
        Highlighter,
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
    // 马赛克纹理基于当前选区生成，并通过 brushOrigin 对齐到选区坐标
    void initMosaicBrush(const QImage &baseImage, const QPoint &brushOrigin = QPoint());
    void setText(const QRectF &rect, const QString& text);
    void setBitmap(const QImage &image, const QRect &rect);
    const QImage& bitmap() const { return bitmap_; }
    QRect bitmapRect() const { return bitmapRect_; }
    void setStamp(::Stamp::Kind kind, const QColor& color) { stampKind_ = kind; stampColor_ = color; }
    ::Stamp::Kind stampKind() const { return stampKind_; }
    QColor stampColor() const { return stampColor_; }
    void updateCursor();
    void clear();
    void draw(QPainter &painter);
    bool isPathShape() const;
    Shape shape() const { return shape_; }
    const QCursor& cursor() const { return cursor_; }

    void initPainter(QPainter& painter);
    void drawPolyLine(const QVector<QPoint> &points, QPainter& painter);
    void drawHighlighter(QPainter& painter);
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
    QPoint brushOrigin_;
    QImage bitmap_;
    QRect bitmapRect_;
    ::Stamp::Kind stampKind_ = ::Stamp::Number;
    QColor stampColor_ = QColor("#DC3545");
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
    void pushBitmap(const QImage &image, const QRect &rect);
    void replaceWithBitmap(const QImage &image, const QRect &rect,
                           std::function<void()> onUndo = {});
    void clearForTransformation(std::function<void()> onUndo);
    void clearHistory();
    
    void drawPixmap(QPixmap &pixmap, const QPoint &offset = QPoint(0, 0));
    void onPaint(QPainter &painter);
    void showTextEdit(const QPoint &pos);
    bool saveText();
    void cancelText();
    void setDrawRect(const QRect &rect);
    int nextStampNumber() const { return nextStampNumber_; }

signals:
    void interactionStarted();

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
    struct UndoState {
        bool replacesModes = false;
        QList<DrawMode> modes;
        QRect drawRect;
        std::function<void()> onUndo;
        int stampNumber = 1;
    };
    QList<UndoState> undoHistory_;
    void rememberState(bool replacesModes = false, std::function<void()> onUndo = {});
    TextEdit *textEdit_;
    QImage bkImage_;
    QRect drawRect_;
    int nextStampNumber_ = 1;
};

//////////////////////////////////////////////////////////////////////////
class DrawPanel : public QWidget {
Q_OBJECT
public:
    // 父窗口、需要绘制的窗口
    DrawPanel(class SettingModel* settings, QWidget *parent, QWidget *drawWidget);
    DrawMode getMode();
    void adjustPos();
    Drawer* drawer();
    void cancelChecked();
    // Show a short status/result banner under the toolbar (e.g. QR scan).
    void showToolMessage(const QString& message, bool isError = false);
    void clearToolMessage();
    static int fontSize();
    static QColor currentColor();

signals:
    void sigSticker();
    void sigLongScreenshot();
    void sigGifRecording();
    void sigOcr();
    void sigSave();
    void sigFinished();
    void sigScanCode();
    void sigSmartMask();
    void sigAutoCrop();
    void sigExtractColors();

public slots:
    void onReferRectChanged(const QRect &rect);
    void onShapeBtnClicked(QAbstractButton*);
    void onColorBtnClicked();
    void onSettingChanged(int fontSize, QColor color);

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    SettingModel* settings_ = nullptr;
    double lastScaleFactor_ = -1.0;
    bool aboveSelection_ = false;
    Q_DISABLE_COPY(DrawPanel)
    void applyCurrentStyle(DrawMode &mode) const;
    void uncheckOtherButtons(QAbstractButton *checkedButton);
    void setupButtonGroup();
    void setupPanels();
    void setupMoreTools(bool hasParent);
    void showMoreTools();
    void setupShapePopup(const QList<QPushButton*>& buttons);
    void syncShapeButton();
    void setupButtonsAndLayout(bool hasParent);
    void refreshPanelSize();
    void setupStampPicker();
    void showStampPicker();

    QRect referRect_;
    QPushButton *pbFont_ = nullptr;
    QPushButton *pbMore_ = nullptr;
    QPushButton *pbShapes_ = nullptr;
    QWidget *shapePopup_ = nullptr;
    QPushButton *pbStamp_ = nullptr;
    QWidget *stampPicker_ = nullptr;
    QHBoxLayout *toolbarLayout_ = nullptr;
    QWidget *moreTools_ = nullptr;
    DrawSettings *drawSettings_ = nullptr;
    QButtonGroup *shapeGroup_ = nullptr;
    QList<QPair<QPushButton*, DrawMode>> btns_;
    QTextEdit *toolMessageLabel_ = nullptr;
    bool messageHovered_ = false;
    QTimer *toolMessageTimer_ = nullptr;
    Drawer drawer_;
};
