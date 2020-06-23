#include "TitleWidget.h"
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>

TitleWidget::TitleWidget(QWidget *parent)
	: QFrame(parent)
{
	this->setObjectName("TitleWidget");
	labelTitle_ = new QLabel(this);
	pbClose_ = new QPushButton(this);
	pbClose_->setObjectName("closeButton");
	connect(pbClose_, &QPushButton::clicked, this, &TitleWidget::sigClose);

	QHBoxLayout *mLayout = new QHBoxLayout(this);
	mLayout->setContentsMargins(6, 0, 6, 0);
	mLayout->setSpacing(0);
	mLayout->addWidget(labelTitle_);
	mLayout->addStretch();
	mLayout->addWidget(pbClose_);

	this->setFixedHeight(30);
}

void TitleWidget::setTitle(const QString &title)
{
	labelTitle_->setText(title);
}
