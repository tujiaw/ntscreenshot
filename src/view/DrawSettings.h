#pragma once

#include <QWidget>

class QComboBox;
class QPushButton;
class DrawSettings : public QWidget
{
    Q_OBJECT
public:
    DrawSettings(QWidget *parent = nullptr);
    static int fontSize();
    static QColor currentColor();

signals:
    void sigChanged(int fontSize, QColor color);

public slots:
    void onFontSizeChanged(const QString &text);
    void onCurrentColor();
    void onColor();

private:
    QComboBox *sizeList_;
    QPushButton *pbCurrentColor_;
    static int s_fontSize;
    static QColor s_currentColor;
};