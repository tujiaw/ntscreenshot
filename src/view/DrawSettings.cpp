#include "DrawSettings.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>

DrawSettings::DrawSettings(QWidget *parent)
    : QWidget(parent)
{
    sizeList_ = new QComboBox(this);
    sizeList_->addItems(QStringList() << "8" << "9" << "10" << "11" << "12" << "14" << "16" << "18" << "20" << "22");
    
    QPushButton *pbCurrentColor = new QPushButton(this);
    QList<QPushButton*> colorBtns;
    for (int i = 0; i < 16; i++) {
        QPushButton *btn = new QPushButton(this);
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
    colorLayout->addWidget(pbCurrentColor);
    colorLayout->addLayout(vLayout);

    mLayout->addLayout(colorLayout);

    this->setFixedSize(300, 50);
}

