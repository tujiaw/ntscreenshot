#include "DrawPanel.h"

#include <QPen>
#include <QPainter>
#include <QPoint>
#include <QPolygon>
#include <QStaticText>
#include <QDebug>
#include <QPushButton>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QColorDialog>
#include <QApplication>
#include <QDesktopWidget>
#include "component/TextEdit.h"
#include "common/Constants.h"
#include "common/Util.h"
#include "DrawSettings.h"

static QColor s_currentColor = Qt::red;
DrawPanel::DrawPanel(QWidget *parent, QWidget *drawWidget)
    : QWidget(parent), drawer_(drawWidget), drawSettings_(nullptr)
{    
    this->setObjectName("DrawPanel");
    QHBoxLayout *hLayout = new QHBoxLayout(this);
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->setSpacing(0);
    
    QButtonGroup* shapeGroup = new QButtonGroup(this);
    shapeGroup->setExclusive(false);
    connect(shapeGroup, SIGNAL(buttonClicked(QAbstractButton*)), this, SLOT(onShapeBtnClicked(QAbstractButton*)));

    auto createLine = [this]() -> QPushButton * {
        QPushButton* pb = new QPushButton("|", this);
        pb->setFocusPolicy(Qt::NoFocus);
        pb->setFixedSize(5, 25);
        return pb;
    };

    auto createShapeBtn = [this](QButtonGroup * shapeGroup, const QString &iconPath, const QString &tooltip) -> QPushButton* {
        QPushButton* pb = new QPushButton(QIcon(iconPath), "", this);
        pb->setToolTip(tooltip);
        pb->setCheckable(true);
        pb->setFixedSize(25, 25);
        shapeGroup->addButton(pb);
        return pb;
    };

    auto createActionBtn = [this](const QString& iconPath, const QString& tooltip) -> QPushButton* {
        QPushButton* pb = new QPushButton(QIcon(iconPath), "", this);
        pb->setToolTip(tooltip);
        pb->setFixedSize(25, 25);
        return pb;
    };

    pbFont_ = new QPushButton(QStringLiteral("Α"), this);
    connect(pbFont_, &QPushButton::clicked, this, &DrawPanel::onColorBtnClicked);
    pbFont_->setStyleSheet(QString("color:%1;font-size:18px;").arg(s_currentColor.name()));
    pbFont_->setFixedSize(25, 25);

    QPushButton* pbPolyLine = createShapeBtn(shapeGroup, ":/images/polyline.png", QStringLiteral("折线"));
    QPushButton* pbLine = createShapeBtn(shapeGroup, ":/images/line.png", QStringLiteral("直线"));
    QPushButton* pbArrow = createShapeBtn(shapeGroup, ":/images/arrow.png", QStringLiteral("箭头"));
    QPushButton* pbRectangle = createShapeBtn(shapeGroup, ":/images/rectangle.png", QStringLiteral("矩形"));
    QPushButton* pbEllipse = createShapeBtn(shapeGroup, ":/images/ellipse.png", QStringLiteral("椭圆"));
    QPushButton* pbText = createShapeBtn(shapeGroup, ":/images/text.png", QStringLiteral("文本"));
    QPushButton* line2 = createLine();
    QPushButton* pbUndo = createActionBtn(":/images/undo.png", QStringLiteral("撤销"));
    QPushButton* pbSticker = createActionBtn(":/images/pin.png", QStringLiteral("贴图"));
    QPushButton* pbSave = createActionBtn(":/images/save.png", QStringLiteral("保存"));
    QPushButton* pbFinished = createActionBtn(":/images/clipboard.png", QStringLiteral("剪切板"));
    connect(pbUndo, &QPushButton::clicked, &drawer_, &Drawer::undo);
    connect(pbSticker, &QPushButton::clicked, this, &DrawPanel::sigSticker);
    connect(pbSave, &QPushButton::clicked, this, &DrawPanel::sigSave);
    connect(pbFinished, &QPushButton::clicked, this, &DrawPanel::sigFinished);
    
    DrawMode polylineMode(DrawMode::PolyLine);
    DrawMode lineMode(DrawMode::Line);
    DrawMode arrowMode(DrawMode::Arrow);
    arrowMode.pen().setStyle(Qt::NoPen);
    arrowMode.brush().setStyle(Qt::SolidPattern);
    DrawMode rectangleMode(DrawMode::Rectangle);
    DrawMode ellipseMode(DrawMode::Ellipse);
    DrawMode textMode(DrawMode::Text);
    btns_.push_back(qMakePair(pbPolyLine, polylineMode));
    btns_.push_back(qMakePair(pbLine, lineMode));
    btns_.push_back(qMakePair(pbArrow, arrowMode));
    btns_.push_back(qMakePair(pbRectangle, rectangleMode));
    btns_.push_back(qMakePair(pbEllipse, ellipseMode));
    btns_.push_back(qMakePair(pbText, textMode));

    hLayout->addWidget(pbFont_);
    for (int i = 0; i < btns_.size(); i++) {
        hLayout->addWidget(btns_[i].first);
    }
    hLayout->addWidget(line2);
    hLayout->addWidget(pbUndo);
    hLayout->addWidget(pbSticker);
    hLayout->addWidget(pbSave);
    hLayout->addWidget(pbFinished);

    if (!parent) {
        setWindowFlags(Qt::ToolTip);
        pbSticker->hide();
    }
}

DrawMode DrawPanel::getMode()
{
    DrawMode mode;
    for (int i = 0; i < btns_.size(); i++) {
        if (!btns_[i].second.isNone()) {
            if (btns_[i].first->isChecked()) {
                mode = btns_[i].second;
                break;
            }
        }
    }
    mode.pen().setColor(s_currentColor);
    mode.brush().setColor(s_currentColor);
    return mode;
}

void DrawPanel::adjustPos()
{
    int x = referRect_.x() + referRect_.width() - this->width();
    int y = referRect_.y() + referRect_.height() + 2;
    if (x + this->width() > QApplication::desktop()->rect().width()) {
        x = QApplication::desktop()->rect().width() - this->width();
    }
    if (y + this->height() > QApplication::desktop()->rect().height()) {
        y = referRect_.y() - this->height() - 2;
    }
    x = qMax(x, 2);
    y = qMax(y, 2);
    this->move(x, y);
}

Drawer* DrawPanel::drawer()
{
    return &drawer_;
}

QColor DrawPanel::currentColor()
{
    return s_currentColor;
}

void DrawPanel::onReferRectChanged(const QRect &rect)
{
    referRect_ = rect;
    adjustPos();
}

void DrawPanel::onShapeBtnClicked(QAbstractButton* btn)
{
    for (int i = 0; i < btns_.size(); i++) {
        if (btns_[i].first != btn && btns_[i].first->isChecked()) {
            btns_[i].first->setChecked(false);
        }
    }

    if (btn->isChecked()) {
        drawer_.setMode(getMode());
    } else {
        drawer_.setMode(DrawMode(DrawMode::None));
    }

    if (drawSettings_ && drawSettings_->isVisible()) {
        delete drawSettings_;
        drawSettings_ = nullptr;
        return;
    }
}

void DrawPanel::onColorBtnClicked()
{
    //QColorDialog dlg(s_currentColor, this);
    //dlg.setStyleSheet("QPushButton{ width: 80px; height: 25px;};");
    //if (QDialog::Accepted == dlg.exec()) {
    //    s_currentColor = dlg.selectedColor();
    //    pbFont_->setStyleSheet(QString("color:%1;font-size:18px;").arg(s_currentColor.name()));
    //    drawer_.setMode(getMode());
    //}

    if (drawSettings_ && drawSettings_->isVisible()) {
        delete drawSettings_;
        drawSettings_ = nullptr;
        return;
    }

    if (!drawSettings_) {
        drawSettings_ = new DrawSettings();
        drawSettings_->setWindowFlags(Qt::ToolTip);
    }

    QPoint pos = pbFont_->mapToGlobal(pbFont_->pos());
    pos.setY(pos.y() + pbFont_->height());
    drawSettings_->move(pos);
    drawSettings_->show();
}

void DrawPanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    adjustPos();
}

void DrawPanel::mouseReleaseEvent(QMouseEvent*event)
{
    event->accept();
}

//////////////////////////////////////////////////////////////////////////
DrawMode::DrawMode() 
    : shape_(None)
    , cursor_(Qt::ArrowCursor)
{
    init();
}

DrawMode::DrawMode(Shape shape)
    : shape_(shape)
{
    init();
}

void DrawMode::init()
{
    pen_.setColor(Qt::red);
    pen_.setWidth(2);
    brush_.setColor(Qt::red);
    brush_.setStyle(Qt::NoBrush);
    if (shape_ == None) {
        cursor_ = Qt::SizeAllCursor;
    } else {
        if (shape_ == Text) {
            cursor_ = Qt::IBeamCursor;
        } else {
            cursor_ = Qt::CrossCursor;
        }
    }
}

bool DrawMode::isNone() const
{
    return shape_ == None;
}

bool DrawMode::isValid() const
{
    if (isNone()) {
        return false;
    }

    if (shape_ == PolyLine) {
        return !points_.isEmpty();
    } else if (shape_ == Text) {
        return !text_.isEmpty();
    } else {
        // 起止距离太短认为是一个无效的绘制
        return (start_ - end_).manhattanLength() > QApplication::startDragDistance();
    }
}

void DrawMode::setPos(const QPoint &start, const QPoint &end)
{
    start_ = start;
    end_ = end;
}

void DrawMode::addPos(const QPoint &pos)
{
    points_.push_back(pos);
}

void DrawMode::setText(const QRectF &rect, const QString& text)
{
    textRect_ = rect;
    text_ = text;
}

void DrawMode::clear()
{
    start_ = QPoint(0, 0);
    end_ = QPoint(0, 0);
    points_.clear();
    text_.clear();
}

void DrawMode::draw(QPainter &painter)
{
    if (!isValid()) {
        return;
    }
    initPainter(painter);
    switch (shape_) {
    case PolyLine:
        drawPolyLine(points_, painter);
        break;
    case Line:
        drawLine(start_, end_, painter);
        break;
    case Arrow:
        drawArrows(start_, end_, painter);
        break;
    case Rectangle:
        drawRect(start_, end_, painter);
        break;
    case Ellipse:
        drawEllipse(start_, end_, painter);
        break;
    case Text:
        drawText(textRect_, text_, painter);
        break;
    }
}

void DrawMode::initPainter(QPainter& painter)
{
    painter.setPen(pen_);
    painter.setBrush(brush_);
    painter.setFont(font_);
    painter.setRenderHint(QPainter::Antialiasing, true);
}

void DrawMode::drawPolyLine(const QVector<QPoint> &points, QPainter& painter)
{
    // 绘制折线
    QPolygon polygon;
    polygon.append(start_);
    polygon.append(points);
    polygon.append(end_);
    painter.drawPolyline(polygon);
}

void DrawMode::drawLine(const QPoint& startPoint, const QPoint& endPoint, QPainter& painter)
{
    // 绘制直线
    painter.drawLine(startPoint, endPoint);
}

void DrawMode::drawArrows(const QPoint& startPoint, const QPoint& endPoint, QPainter &painter)
{
    // 箭头部分三角形的腰长
    double par = 15.0;
    double slopy = atan2((endPoint.y() - startPoint.y()), (endPoint.x() - startPoint.x()));
    double cos_y = cos(slopy);
    double sin_y = sin(slopy);
    QPoint head_point1 = QPoint(endPoint.x() + int(-par*cos_y - (par / 2.0 * sin_y)), endPoint.y() + int(-par*sin_y + (par / 2.0 * cos_y)));
    QPoint head_point2 = QPoint(endPoint.x() + int(-par*cos_y + (par / 2.0 * sin_y)), endPoint.y() - int(par / 2.0*cos_y + par * sin_y));
    QPoint head_points[3] = { endPoint, head_point1, head_point2 };
    // 绘制箭头部分
    painter.drawPolygon(head_points, 3);
    // 计算箭身部分
    int offset_x = int(par*sin_y / 3);
    int offset_y = int(par*cos_y / 3);
    QPoint body_point1, body_point2;
    body_point1 = QPoint(endPoint.x() + int(-par*cos_y - (par / 2.0*sin_y)) + offset_x, endPoint.y() + int(-par*sin_y + (par / 2.0*cos_y)) - offset_y);
    body_point2 = QPoint(endPoint.x() + int(-par*cos_y + (par / 2.0*sin_y) - offset_x), endPoint.y() - int(par / 2.0*cos_y + par*sin_y) + offset_y);
    QPoint body_points[3] = { startPoint, body_point1, body_point2 };
    // 绘制箭身部分
    painter.drawPolygon(body_points, 3);
}

void DrawMode::drawRect(const QPoint &startPoint, const QPoint &endPoint, QPainter &painter) 
{
    // 绘制矩形
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRect(startPoint, endPoint));
}

void DrawMode::drawEllipse(const QPoint &startPoint, const QPoint &endPoint, QPainter &painter) 
{
    // 绘制椭圆
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QRect(startPoint, endPoint));
}

void DrawMode::drawText(const QPoint& startPoint, const QString& text, QPainter& painter)
{
    painter.drawStaticText(startPoint, QStaticText(text));
}

void DrawMode::drawText(const QRectF &rectangle, const QString& text, QPainter& painter)
{
    painter.drawText(rectangle, Qt::AlignLeft, text);
}

////////////////////////////////////////////////////////////////////////////
Drawer::Drawer(QWidget* parent)
    : QObject(parent), parent_(parent), isPressed_(false), isEnabled_(false), textEdit_(nullptr)
{
    parent_->installEventFilter(this);
}

Drawer::~Drawer()
{
    // 防止drawer销毁后cursor没有重置
    setMode(DrawMode(DrawMode::None));
}

QWidget* Drawer::parentWidget()
{
    return parent_;
}

void Drawer::setEnable(bool enable)
{
    isEnabled_ = enable;
}

bool Drawer::enable() const
{
    return isEnabled_;
}

void Drawer::setMode(const DrawMode &drawMode)
{
    saveText();
    parent_->setCursor(drawMode.cursor());
    drawMode_ = drawMode;
}

const DrawMode& Drawer::mode() const
{
    return drawMode_;
}

bool Drawer::isDraw() const
{
    return isEnabled_ && !drawMode_.isNone();
}

void Drawer::undo()
{
    drawStartPos_ = QPoint(0, 0);
    drawEndPos_ = QPoint(0, 0);
    if (!drawModeCache_.isEmpty()) {
        drawModeCache_.pop_back();
        parent_->update();
    }
}

void Drawer::onPaint(QPainter &painter)
{
    // 当前绘制
    if (isDraw()) {
        drawMode_.setPos(drawStartPos_, drawEndPos_);
        drawMode_.draw(painter);
    }

    // 绘制缓存
    for (int i = 0; i < drawModeCache_.size(); i++) {
        DrawMode& dm = drawModeCache_[i];
        dm.draw(painter);
    }
}

void Drawer::showTextEdit(const QPoint &pos)
{
    if (!textEdit_) {
        textEdit_ = new TextEdit(parent_);
    }

    textEdit_->setFont(drawMode_.font());
    textEdit_->setStyle(drawMode_.pen().color());
    textEdit_->move(pos - QPoint(2, 12));
    textEdit_->setFocus();
    textEdit_->show();
}

void Drawer::saveText()
{
    if (textEdit_ && textEdit_->isVisible()) {
        QString text = textEdit_->toPlainText().trimmed();
        if (!text.isEmpty()) {
            QPoint start = textEdit_->startCursorPoint();
            drawMode_.setText(QRectF(start.x(), start.y(), textEdit_->width(), textEdit_->height()), text);
            drawModeCache_.push_back(drawMode_);
        }
        drawMode_.clear();
        textEdit_->clear();
        textEdit_->setVisible(false);
    }
}

void Drawer::drawPixmap(QPixmap &pixmap)
{
    saveText();
    if (!drawModeCache_.isEmpty()) {
        QPainter painter(&pixmap);
        for (int i = 0; i < drawModeCache_.size(); i++) {
            DrawMode &dm = drawModeCache_[i];
            dm.draw(painter);
        }
    }
}

bool Drawer::eventFilter(QObject* watched, QEvent* event)
{
    if (parent_ == watched) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            return onMousePressEvent(mouseEvent);
        } else if (event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            return onMouseReleaseEvent(mouseEvent);
        } else if (event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            return onMouseMoveEvent(mouseEvent);
        }
    }
    return false;
}

bool Drawer::onMousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        isPressed_ = true;
        drawStartPos_ = e->pos();
        drawEndPos_ = e->pos();
    }
    return false;
}

bool Drawer::onMouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        // 绘制文本
        if (isDraw()) {
            saveText();
            if (drawMode_.shape() == DrawMode::Text) {
                showTextEdit(e->pos());
            }
        }

        // 当前绘制结束，存档、清理
        if (isPressed_) {
            drawMode_.setPos(drawStartPos_, e->pos());
            if (isEnabled_ && drawMode_.isValid()) {
                drawModeCache_.push_back(drawMode_);
            }
            drawMode_.clear();
        }

        drawStartPos_ = QPoint(0, 0);
        drawEndPos_ = QPoint(0, 0);
        isPressed_ = false;
    }
    return false;
}

bool Drawer::onMouseMoveEvent(QMouseEvent *e)
{
    QCursor newCursor = isDraw() ? drawMode_.cursor() : Qt::SizeAllCursor;
    if (parent_->cursor().shape() != newCursor.shape()) {
        parent_->setCursor(newCursor);
    }
    
    if (isPressed_ && isDraw()) {
        // 进入绘制模式
        drawEndPos_ = e->pos();
        // 绘制折线保存鼠标移动的每一个点
        if (drawMode_.shape() == DrawMode::PolyLine) {
            drawMode_.addPos(e->pos());
        }
        parent_->update();
        return true;
    }
    return false;
}
