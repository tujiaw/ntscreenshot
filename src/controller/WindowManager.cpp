#include "WindowManager.h"
#include <QTimer>
#include <QEventLoop>
#include "component/TitleWidget.h"
#include "component/FramelessWidget.h"
#include "view/MainWidget.h"
#include "view/Screenshot.h"
#include "view/Settings.h"
#include "view/Stickers.h"

WindowManager::WindowManager()
{
    settingModel_.reset(new SettingModel(nullptr));
    connect(this, &WindowManager::sigPin, this, &WindowManager::onPin);
}

WindowManager::~WindowManager()
{
	destory();
}

WindowManager* WindowManager::instance()
{
	static WindowManager s_inst;
	return &s_inst;
}

void WindowManager::destory()
{
	for (auto iter = widgets_.begin(); iter != widgets_.end(); ++iter) {
		(*iter)->close();
	}
	widgets_.clear();
}

void WindowManager::openWidget(const QString &id)
{
	if (id == WidgetID::SCREENSHOT) {
		screenshot_.reset();
        QEventLoop loop;
        loop.processEvents();
        screenshot_.reset(new ScreenshotWidget());
        ScreenshotWidget::setPinGlobalKey(settingModel_->pinGlobalKey());
        ScreenshotWidget::setUploadImageUrl(settingModel_->uploadImageUrl());
        connect(screenshot_.get(), &ScreenshotWidget::sigReopen, this, &WindowManager::onScreenshotReopen, Qt::QueuedConnection);
        connect(screenshot_.get(), &ScreenshotWidget::sigClose, this, &WindowManager::onScreenshotClose, Qt::QueuedConnection);
		return;
	}
	else if (id == WidgetID::MAIN) {
		if (!widgets_.contains(id)) {
			FramelessWidget* widget = new FramelessWidget();
			MainWidget* content = new MainWidget(widget);
			widget->setContent(content);
			widget->hide();
			widgets_[id] = widget;
		}
	}
	else if (id == WidgetID::SETTINGS) {
		QWidget* widget = nullptr;
		if (widgets_.contains(id)) {
			widget = widgets_[id];
		}
		else {
			FramelessWidget* frame = new FramelessWidget();
			widget = frame;
			TitleWidget* title = new TitleWidget(frame);
			title->setTitle(QStringLiteral("ÉèÖÃ"));
			connect(title, &TitleWidget::sigClose, [this]() {
				WindowManager::instance()->closeWidget(WidgetID::SETTINGS);
			});
			Settings* content = new Settings(widget);
			frame->setTitle(title);
			frame->setContent(content);
			widgets_[id] = widget;
		}
		widget->show();
		widget->raise();
	}
}

void WindowManager::closeWidget(const QString &id)
{
    if (id == WidgetID::SCREENSHOT) {
        screenshot_->close();
        screenshot_.reset();
        return;
    }

	if (widgets_.contains(id)) {
		widgets_[id]->close();
		widgets_.remove(id);
	}
}

void WindowManager::hideWidget(const QString &id)
{
	if (widgets_.contains(id)) {
		widgets_[id]->hide();
	}
}

QWidget* WindowManager::content(const QString &id)
{
    if (widgets_.contains(id)) {
        FramelessWidget *parent = qobject_cast<FramelessWidget*>(widgets_[id]);
        if (parent) {
            return parent->getContent();
        }
    }
    return nullptr;
}

void WindowManager::onScreenshotReopen()
{
    openWidget(WidgetID::SCREENSHOT);
}

void WindowManager::onScreenshotClose()
{
    closeWidget(WidgetID::SCREENSHOT);
}

void WindowManager::onPin()
{
    if (screenshot_) {
        screenshot_->pin();
    } else {
        if (StickerWidget::allCount() == StickerWidget::visibleCount()) {
            StickerWidget::hideAll();
        } else {
            StickerWidget::showAll();
        }
    }
}

bool WindowManager::setScreenshotGlobalKey(const QString &key)
{
    MainWidget *mainWidget = qobject_cast<MainWidget*>(content(WidgetID::MAIN));
    if (mainWidget) {
        return mainWidget->setScreenshotGlobalKey(key);
    }
    return false;
}

bool WindowManager::setPinGlobalKey(const QString &key)
{
    MainWidget *mainWidget = qobject_cast<MainWidget*>(content(WidgetID::MAIN));
    if (mainWidget) {
        return mainWidget->setPinGlobalKey(key);
    }
    return false;
}

void WindowManager::showAllSticker()
{
    StickerWidget::showAll();
}

int WindowManager::allStickerCount()
{
    return StickerWidget::allCount();
}
