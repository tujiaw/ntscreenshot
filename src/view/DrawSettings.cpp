#include "DrawSettings.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QColorDialog>
#include <QKeyEvent>

int DrawSettings::s_fontSize = 12;
QColor DrawSettings::s_currentColor = QColor("#FF0000");
DrawSettings::DrawSettings(QWidget *parent)
    : QWidget(parent)
{
    sizeList_ = new QComboBox(this);
    sizeList_->addItems(QStringList() << "8" << "9" << "10" << "11" << "12" << "14" << "16" << "18" << "20" << "22");
    sizeList_->setCurrentText(QString::number(s_fontSize));
    connect(sizeList_, &QComboBox::currentTextChanged, this, &DrawSettings::onFontSizeChanged);
    
    pbCurrentColor_ = new QPushButton(this);
    pbCurrentColor_->setFixedSize(26, 26);
    pbCurrentColor_->setStyleSheet(QString("background-color:%1").arg(s_currentColor.name()));
    connect(pbCurrentColor_, &QPushButton::clicked, this, &DrawSettings::onCurrentColor);

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
    mLayout->addWidget(sizeList_);

    QHBoxLayout *firstLineLayout = new QHBoxLayout(this);
    firstLineLayout->setSpacing(2);
    for (int i = 0; i < 8; i++) {
        firstLineLayout->addWidget(colorBtns[i]);
    }
    QHBoxLayout *secondLineLayout = new QHBoxLayout(this);
    secondLineLayout->setSpacing(2);
    for (int i = 8; i < 16; i++) {
        secondLineLayout->addWidget(colorBtns[i]);
    }
    QVBoxLayout *vLayout = new QVBoxLayout(this);
    vLayout->setSpacing(2);
    vLayout->addLayout(firstLineLayout);
    vLayout->addLayout(secondLineLayout);

    QHBoxLayout *colorLayout = new QHBoxLayout(this);
    colorLayout->setContentsMargins(0, 0, 0, 0);
    colorLayout->setSpacing(2);
    colorLayout->addWidget(pbCurrentColor_);
    colorLayout->addLayout(vLayout);

    mLayout->addLayout(colorLayout);

    this->setFixedSize(226, 38);
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
        emit sigChanged(s_fontSize, s_currentColor);
    }
}

void DrawSettings::onCurrentColor()
{
    QColorDialog dlg(s_currentColor, this);
    dlg.setStyleSheet("QPushButton{ width: 80px; height: 25px;};");
    if (QDialog::Accepted == dlg.exec()) {
        s_currentColor = dlg.selectedColor();
        pbCurrentColor_->setStyleSheet(QString("background-color:%1").arg(s_currentColor.name()));
        emit sigChanged(s_fontSize, s_currentColor);
    }
}

void DrawSettings::onColor()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (btn) {
        QString color = btn->property("color").toString();
        s_currentColor = QColor(color);
        pbCurrentColor_->setStyleSheet(QString("background-color:%1").arg(s_currentColor.name()));
        emit sigChanged(s_fontSize, s_currentColor);
    }
}

void DrawSettings::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        hide();
    }
    QWidget::keyPressEvent(event);
}
