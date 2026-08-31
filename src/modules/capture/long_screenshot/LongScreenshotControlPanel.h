#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QString>

class LongScreenshotControlPanel : public QWidget
{
    Q_OBJECT

public:
    explicit LongScreenshotControlPanel(QWidget *parent = nullptr);
    ~LongScreenshotControlPanel();

    void setInfoText(const QString &text);

protected:
    void paintEvent(QPaintEvent *event) override;

signals:
    void sigCancelClicked();
    void sigFinishClicked();

private:
    void setupUI();

    QLabel* infoLabel_;
    QPushButton* cancelButton_;
    QPushButton* finishButton_;
};
