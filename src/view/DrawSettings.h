#pragma once

#include <QWidget>

class QComboBox;
class QPushButton;
class DrawSettings : public QWidget
{
    Q_OBJECT
public:
    DrawSettings(QWidget *parent = nullptr);
    static int penWidth();
    static int fontSize();
    static QColor currentColor();

signals:
    void sigChanged(int penWidth, int fontSize, QColor color);

public slots:
    void onFontSizeChanged(const QString &text);
    void onCurrentColor();
    void onColor();
    void onPenWidthSelected();

private:
    QComboBox *sizeList_;
    QPushButton *pbCurrentColor_;
    QList<QPushButton*> penWidthBtns_;
};