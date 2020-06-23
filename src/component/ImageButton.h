#pragma once

#include <QPushButton>
#include <QToolButton>

class ImageButton : public QPushButton
{
	Q_OBJECT
public:
	enum Status { NORMAL = 0, ENTER, PRESS, RELEASE, STATUS_NUM, STATUS_NONE };

	explicit ImageButton(QWidget *parent = 0);
	~ImageButton();

	void setPicName(const QString &normal, const QString &hover, const QString &pressed, const QString &disable=QString());
	void setImages(const QPixmap &normal, const QPixmap &hover, const QPixmap &pressed, const QPixmap &disable=QPixmap());
	void setNumberPicName(const QString &imagePath);

	void setControlStatus(Status status = STATUS_NONE);
	void setNumber(int number);
	int getNumber() const;

protected:
	void enterEvent(QEvent *event);
	void leaveEvent(QEvent *event);
	void mousePressEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void paintEvent(QPaintEvent *event);

private:
	Status status_;
	Status controlStatus_;
	QPixmap pixmaps_[STATUS_NUM];
	QPixmap numberPixmap_;
	bool isMousePressed_;
	int number_;
};
