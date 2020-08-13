#pragma once

#include <QWidget>

class QComboBox;
class DrawSettings : public QWidget
{
    Q_OBJECT
public:
    DrawSettings(QWidget *parent = nullptr);

private:
    QComboBox *sizeList_;
};