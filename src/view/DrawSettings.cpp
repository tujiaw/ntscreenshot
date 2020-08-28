#include "DrawSettings.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QColorDialog>
#include <QKeyEvent>

const QList<int> s_penWidthList = { 1, 3, 6 };
static int s_penWidth = s_penWidthList[1];
static int s_fontSize = 12;
static QColor s_currentColor = QColor("#FF0000");

DrawSettings::DrawSettings(QWidget *parent)
    : QWidget(parent)
{
    this->setAutoFillBackground(true);

    // draw pen width
    for (int i = 0; i < s_penWidthList.size(); i++){
        int w = s_penWidthList.at(i);
        QPushButton *btn = new QPushButton(this);
        connect(btn, &QPushButton::clicked, this, &DrawSettings::onPenWidthSelected);
        btn->setFixedSize(w*2 + 5, w*2 + 5);
        btn->setCheckable(true);
        btn->setProperty("value", w);
        QStringList styleList;
        styleList << QString("QPushButton{background-color:#fff;margin:1px;}");
        styleList << QString("QPushButton::hover{margin: 0px;}");
        styleList << QString("QPushButton::checked{margin: 0px; border:2px solid #808080;}");
        btn->setStyleSheet(styleList.join(""));
        btn->setChecked(w == s_penWidth);
        penWidthBtns_.push_back(btn);
    }
    onPenWidthSelected();

    // font size
    sizeList_ = new QComboBox(this);
    sizeList_->addItems(QStringList() << "8" << "9" << "10" << "11" << "12" << "14" << "16" << "18" << "20" << "22");
    sizeList_->setCurrentText(QString::number(s_fontSize));
    connect(sizeList_, &QComboBox::currentTextChanged, this, &DrawSettings::onFontSizeChanged);
    
    // current color
    pbCurrentColor_ = new QPushButton(this);
    pbCurrentColor_->setFixedSize(26, 26);
    pbCurrentColor_->setStyleSheet(QString("background-color:%1").arg(s_currentColor.name()));
    connect(pbCurrentColor_, &QPushButton::clicked, this, &DrawSettings::onCurrentColor);

    // color select
    const QStringList colorList = QStringList() << "#000000" << "#808080" << "#800000" << "#F7883A" << "#308430" << "#385AD3" << "#800080" << "#009999"
        << "#FFFFFF" << "#C0C0C0" << "#FB3838" << "#FFFF00" << "#99CC00" << "#3894E4" << "#F31BF3" << "#16DCDC";
    QList<QPushButton*> colorBtns;
    for (int i = 0; i < colorList.size(); i++) {
        QPushButton *btn = new QPushButton(this);
        btn->setFixedSize(14, 14);
        btn->setProperty("color", colorList[i]);
        btn->setStyleSheet(QString("QPushButton{background-color:%1; border:1px solid #808080;margin:1px;}QPushButton::hover{margin:0px;}").arg(colorList[i]));
        connect(btn, &QPushButton::clicked, this, &DrawSettings::onColor);
        colorBtns.push_back(btn);
    }

    QHBoxLayout *mLayout = new QHBoxLayout(this);
    mLayout->setContentsMargins(6, 6, 6, 6);
    mLayout->setSpacing(6);
    for (int i = 0; i < penWidthBtns_.size(); i++) {
        mLayout->addWidget(penWidthBtns_[i]);
    }
    mLayout->addWidget(sizeList_);

    QHBoxLayout *firstLineLayout = new QHBoxLayout();
    firstLineLayout->setSpacing(2);
    for (int i = 0; i < 8; i++) {
        firstLineLayout->addWidget(colorBtns[i]);
    }
    QHBoxLayout *secondLineLayout = new QHBoxLayout();
    secondLineLayout->setSpacing(2);
    for (int i = 8; i < 16; i++) {
        secondLineLayout->addWidget(colorBtns[i]);
    }
    QVBoxLayout *vLayout = new QVBoxLayout();
    vLayout->setSpacing(2);
    vLayout->addLayout(firstLineLayout);
    vLayout->addLayout(secondLineLayout);

    QHBoxLayout *colorLayout = new QHBoxLayout();
    colorLayout->setContentsMargins(0, 0, 0, 0);
    colorLayout->setSpacing(2);
    colorLayout->addWidget(pbCurrentColor_);
    colorLayout->addLayout(vLayout);

    mLayout->addLayout(colorLayout);

    this->setFixedSize(250, 38);
}

int DrawSettings::penWidth()
{
    return s_penWidth;
}

int DrawSettings::fontSize()
{
    return s_fontSize;
}

QColor DrawSettings::currentColor()
{
    return s_currentColor;
}

void DrawSettings::onFontSizeChanged(const QString &text)
{
    bool ok = false;
    int size = text.toInt(&ok);
    if (ok) {
        s_fontSize = size;
        emit sigChanged(s_penWidth, s_fontSize, s_currentColor);
    }
}

void DrawSettings::onCurrentColor()
{
    QColorDialog dlg(s_currentColor, this);
    dlg.setStyleSheet("QPushButton{ width: 80px; height: 25px;};");
    if (QDialog::Accepted == dlg.exec()) {
        s_currentColor = dlg.selectedColor();
        pbCurrentColor_->setStyleSheet(QString("background-color:%1").arg(s_currentColor.name()));
        emit sigChanged(s_penWidth, s_fontSize, s_currentColor);
    }
}

void DrawSettings::onColor()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (btn) {
        QString color = btn->property("color").toString();
        s_currentColor = QColor(color);
        pbCurrentColor_->setStyleSheet(QString("background-color:%1").arg(s_currentColor.name()));
        emit sigChanged(s_penWidth, s_fontSize, s_currentColor);
    }
}

void DrawSettings::onPenWidthSelected()
{
    QPushButton* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) {
        return;
    }

    btn->setChecked(true);
    s_penWidth = btn->property("value").toInt();
    for (int i = 0; i < penWidthBtns_.size(); i++) {
        if (penWidthBtns_[i] != btn) {
            penWidthBtns_[i]->setChecked(false);
        }
    }
    emit sigChanged(s_penWidth, s_fontSize, s_currentColor);
}
