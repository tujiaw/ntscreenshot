#include "DrawPanel.h"

#include <QPen>
#include <QPainter>
#include <QPoint>
#include <QPolygon>
#include <QDebug>
#include <QPushButton>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QColorDialog>
#include <QApplication>
#include <QDesktopWidget>
#include "common/Constants.h"
#include "common/Util.h"

static QColor s_currentColor = Qt::red;
DrawPanel::DrawPanel(QWidget *parent)
    : QWidget(parent)
{
    this->setObjectName("DrawPanel");
    QHBoxLayout *hLayout = new QHBoxLayout(this);
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->setSpacing(0);
    
    QButtonGroup* shapeGroup = new QButtonGroup(this);
    shapeGroup->setExclusive(true);

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
        connect(pb, &QPushButton::clicked, this, &DrawPanel::onShapeBtnClicked);
        shapeGroup->addButton(pb);
        return pb;
    };

    auto createActionBtn = [this](const QString& iconPath, const QString& tooltip) -> QPushButton* {
        QPushButton* pb = new QPushButton(QIcon(iconPath), "", this);
        pb->setToolTip(tooltip);
        pb->setFixedSize(25, 25);
        return pb;
    };

    pbColor_ = new QPushButton(QStringLiteral("Α"), this);
    connect(pbColor_, &QPushButton::clicked, this, &DrawPanel::onColorBtnClicked);
    pbColor_->setStyleSheet(QString("color:%1;font-size:18px;").arg(s_currentColor.name()));
    pbColor_->setFixedSize(25, 25);

    QPushButton* pbPolyLine = createShapeBtn(shapeGroup, ":/images/polyline.png", QStringLiteral("折线"));
    QPushButton* pbLine = createShapeBtn(shapeGroup, ":/images/line.png", QStringLiteral("直线"));
    QPushButton* pbArrow = createShapeBtn(shapeGroup, ":/images/arrow.png", QStringLiteral("箭头"));
    QPushButton* pbRectangle = createShapeBtn(shapeGroup, ":/images/rectangle.png", QStringLiteral("矩形"));
    QPushButton* pbEllipse = createShapeBtn(shapeGroup, ":/images/ellipse.png", QStringLiteral("椭圆"));
    QPushButton* line2 = createLine();
    QPushButton* pbUndo = createActionBtn(":/images/undo.png", QStringLiteral("撤销"));
    QPushButton* pbSticker = createActionBtn(":/images/pin.png", QStringLiteral("贴图"));
    QPushButton* pbSave = createActionBtn(":/images/save.png", QStringLiteral("保存"));
    QPushButton* pbFinished = createActionBtn(":/images/clipboard.png", QStringLiteral("剪切板"));
    connect(pbUndo, &QPushButton::clicked, this, &DrawPanel::sigUndo);
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
    btns_.push_back(qMakePair(pbPolyLine, polylineMode));
    btns_.push_back(qMakePair(pbLine, lineMode));
    btns_.push_back(qMakePair(pbArrow, arrowMode));
    btns_.push_back(qMakePair(pbRectangle, rectangleMode));
    btns_.push_back(qMakePair(pbEllipse, ellipseMode));

    hLayout->addWidget(pbColor_);
    for (int i = 0; i < btns_.size(); i++) {
        hLayout->addWidget(btns_[i].first);
    }
    hLayout->addWidget(line2);
    hLayout->addWidget(pbUndo);
    hLayout->addWidget(pbSticker);
    hLayout->addWidget(pbSave);
    hLayout->addWidget(pbFinished);
}

DrawMode DrawPanel::getMode()
{
    DrawMode mode;
    for (int i = 0; i < btns_.size(); i++) {
        if (!btns_[i].second.isNone()) {
            if (btns_[i].first->isChecked()) {
                mode = btns_[i].second;
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

QColor DrawPanel::currentColor()
{
    return s_currentColor;
}

void DrawPanel::onReferRectChanged(const QRect &rect)
{
    referRect_ = rect;
    adjustPos();
}

void DrawPanel::onShapeBtnClicked()
{
    emit sigStart();
}

void DrawPanel::onColorBtnClicked()
{
    QColorDialog dlg(s_currentColor, this);
    dlg.setStyleSheet("QPushButton{ width: 80px; height: 25px;};");
    if (QDialog::Accepted == dlg.exec()) {
        s_currentColor = dlg.selectedColor();
        pbColor_->setStyleSheet(QString("color:%1;font-size:18px;").arg(s_currentColor.name()));
        emit sigStart();
    }
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
    } else {
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

void DrawMode::clear()
{
    start_ = QPoint(0, 0);
    end_ = QPoint(0, 0);
    points_.clear();
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
    }
}

void DrawMode::initPainter(QPainter& painter)
{
    painter.setPen(pen_);
    painter.setBrush(brush_);
    painter.setRenderHint(QPainter::Antialiasing, true);
}

void DrawMode::drawPolyLine(const QVector<QPoint> &points, QPainter& painter)
{
    QPolygon polygon;
    polygon.append(start_);
    polygon.append(points);
    polygon.append(end_);
    painter.drawPolyline(polygon);
}

void DrawMode::drawLine(const QPoint& startPoint, const QPoint& endPoint, QPainter& painter)
{
    painter.drawLine(startPoint, endPoint);
}

void DrawMode::drawArrows(const QPoint& startPoint, const QPoint& endPoint, QPainter &painter)
{
    /// 箭头部分三角形的腰长
    double par = 15.0;
    double slopy = atan2((endPoint.y() - startPoint.y()),
                         (endPoint.x() - startPoint.x()));
    double cos_y = cos(slopy);
    double sin_y = sin(slopy);
    QPoint head_point1 = QPoint(endPoint.x() + int(-par*cos_y - (par / 2.0 * sin_y)),
                           endPoint.y() + int(-par*sin_y + (par / 2.0 * cos_y)));
    QPoint head_point2 = QPoint(endPoint.x() + int(-par*cos_y + (par / 2.0 * sin_y)),
                           endPoint.y() - int(par / 2.0*cos_y + par * sin_y));
    QPoint head_points[3] = { endPoint, head_point1, head_point2 };
    /// 绘制箭头部分
    painter.drawPolygon(head_points, 3);

    /// 计算箭身部分
    int offset_x = int(par*sin_y / 3);
    int offset_y = int(par*cos_y / 3);
    QPoint body_point1, body_point2;
    body_point1 = QPoint(endPoint.x() + int(-par*cos_y - (par / 2.0*sin_y)) +
                    offset_x, endPoint.y() + int(-par*sin_y + (par / 2.0*cos_y)) - offset_y);
    body_point2 = QPoint(endPoint.x() + int(-par*cos_y + (par / 2.0*sin_y) - offset_x),
                    endPoint.y() - int(par / 2.0*cos_y + par*sin_y) + offset_y);
    QPoint body_points[3] = { startPoint, body_point1, body_point2 };
    /// 绘制箭身部分
    painter.drawPolygon(body_points, 3);

}

void DrawMode::drawRect(const QPoint &startPoint, const QPoint &endPoint, QPainter &painter) {
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRect(startPoint, endPoint));
}

void DrawMode::drawEllipse(const QPoint &startPoint, const QPoint &endPoint, QPainter &painter) {
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QRect(startPoint, endPoint));
}

bool DrawMode::operator==(const DrawMode& other) const
{
    return this->shape_ == other.shape_ &&
        this->start_ == other.start_ &&
        this->end_ == other.end_ &&
        this->pen_ == other.pen_ &&
        this->brush_ == other.brush_;
}
