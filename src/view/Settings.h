#pragma once

#include <QWidget>
#include "ui_Settings.h"

class Settings : public QWidget
{
	Q_OBJECT

public:
	Settings(QWidget *parent = Q_NULLPTR);
	~Settings();
    void readData();
    void writeData();

protected:
	virtual void keyPressEvent(QKeyEvent* event);

private:
    void initTablePath();
    void updateScreenshotGlobalKey(const QString &key);
    void updatePinKey(const QString &key);
    void onAutoStartClicked(bool checked);
    void onPinNoBorder(bool checked);
    void onRevertClicked();
    void onTablePathDoubleClicked(int row, int col);
    void onUploadImageUrlChanged(const QString& text);
    void onRgbColorToggled(bool checked);
	void onHexColorToggled(bool checked);
    void onAutoSaveChanged();
    void onOpenImagePath();
    void onModifyImagePath();

private:
    Q_DISABLE_COPY(Settings)
	Ui::Settings ui;
};
