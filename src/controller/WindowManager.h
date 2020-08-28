#pragma once
#include <memory>
#include <QObject>
#include <QMap>
#include <QPointer>
#include "common/Constants.h"
#include "model/SettingModel.h"

class ScreenshotWidget;
class WindowManager : public QObject
{
	Q_OBJECT

public:
	static WindowManager* instance();
	void destory();
	void openWidget(const QString &id);
	void closeWidget(const QString &id);
	void hideWidget(const QString &id);
    QWidget* content(const QString &id);

    SettingModel* setting() { return settingModel_.get(); }
    bool setScreenshotGlobalKey(const QString &key);
    bool setPinGlobalKey(const QString &key);
    void showAllSticker();
    int allStickerCount();

signals:
    void sigPin();
    void sigSettingChanged();
    void sigStickerCountChanged();

private slots:
    void onScreenshotReopen();
    void onScreenshotClose();
	void onSaveScreenshot(const QPixmap &pixmap);
    void onPin();

private:
	WindowManager();
	~WindowManager();

private:
	QMap<QString, QWidget*> widgets_;
	std::unique_ptr<ScreenshotWidget> screenshot_;
    std::unique_ptr<SettingModel> settingModel_;
};
