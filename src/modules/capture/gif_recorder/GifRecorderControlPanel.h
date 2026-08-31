#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QToolButton;

class GifRecorderControlPanel : public QWidget
{
    Q_OBJECT

public:
    explicit GifRecorderControlPanel(QWidget *parent = nullptr);

    int framesPerSecond() const;
    void setCaptureSize(const QSize &size);
    void setElapsedMilliseconds(qint64 milliseconds, int frames);
    void setPreparing();
    void setStarting();
    void setRecording();
    void setEncoding();

signals:
    void sigRecordClicked();
    void sigStopClicked();
    void sigCancelClicked();

protected:
    void paintEvent(QPaintEvent*) override;
    void applyTheme();

    QLabel *sizeLabel_ = nullptr;
    QLabel *elapsedLabel_ = nullptr;
    QLineEdit *fpsEdit_ = nullptr;
    QPushButton *recordButton_ = nullptr;
    QPushButton *stopButton_ = nullptr;
    QToolButton *cancelButton_ = nullptr;
};
