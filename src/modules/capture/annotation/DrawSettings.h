#pragma once

#include <QWidget>
#include <QList>

class QComboBox;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QShowEvent;
class DrawSettings : public QWidget
{
    Q_OBJECT
public:
    DrawSettings(QWidget *parent = nullptr);
    static int penWidth();
    static int fontSize();
    static QColor currentColor();

    // 按当前屏幕 DPI 重新应用所有尺寸/样式（构造函数与 DPI 变化时调用）
    void rescaleForDpi();

signals:
    void sigChanged(int penWidth, int fontSize, QColor color);

public slots:
    void onFontSizeChanged(const QString &text);
    void onCurrentColor();
    void onColor();
    void onPenWidthSelected();

protected:
    void showEvent(QShowEvent *event) override;

private:
    void refreshCurrentColorButton();
    void syncPresetSelection();

    QLabel *penWidthLabel_ = nullptr;
    QLabel *colorLabel_ = nullptr;
    QComboBox *sizeList_ = nullptr;
    QPushButton *pbCurrentColor_ = nullptr;
    QList<QPushButton*> penWidthBtns_;
    QList<QPushButton*> colorBtns_;
    QHBoxLayout *row1_ = nullptr;
};
