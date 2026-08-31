#pragma once

#include <QFrame>

class QEnterEvent;

class FramelessWidget : public QFrame
{
	Q_OBJECT
public:
    enum Direction { UP = 0, DOWN, LEFT, RIGHT, LEFTTOP, LEFTBOTTOM, RIGHTBOTTOM, RIGHTTOP, MIDDLE };

	FramelessWidget(QWidget *parent = Q_NULLPTR);
	~FramelessWidget();
	void setTitle(QWidget *title);
	void setContent(QWidget *content);
	QWidget* getContent();
    void setEnableStretch(bool enable);
    void setStretchBorderWidth(int width);
	void setEnableEscClose(bool enable);
	void setEnableHighlight(bool enable);
	bool enableHightlight() const;
    void setBackground(const QPixmap &pixmap);
    void setActiveStyle();
    void setDeactiveStyle();

protected:
	void mousePressEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void enterEvent(QEnterEvent* event) override;
	void leaveEvent(QEvent* event);
	void mouseMoveEvent(QMouseEvent *event);
	void keyPressEvent(QKeyEvent* event);
	bool nativeEvent(const QByteArray & eventType, void * message, qintptr * result) override;
	bool eventFilter(QObject* watched, QEvent* event);
    void transRegion(const QPoint &cursorGlobalPoint);
    //void paintEvent(QPaintEvent *event);

private:
	QPoint movePoint_;
	bool isPressed_;
	QWidget *title_;
	QWidget *content_;
	bool enableStretch_;
    int stretchBorderWidth_;
	bool enableEscClose_;
	bool enableHighlight_;
    Direction m_dir;
    QPixmap backgroundPixmap_;
};
