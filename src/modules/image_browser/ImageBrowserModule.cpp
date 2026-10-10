#include "ImageBrowserModule.h"
#include "ImageBrowserWindow.h"
#include "ImageLoader.h"
#include <QCoreApplication>
#include <QEvent>

namespace ImageBrowser {
ImageBrowserModule::ImageBrowserModule(SettingModel* settings)
    : settings_(settings), loader_(new ImageLoader(this)) {}
void ImageBrowserModule::initialize() { qApp->installEventFilter(this); }
bool ImageBrowserModule::eventFilter(QObject* object, QEvent* event) {
    if (object == qApp && event->type() == QEvent::Quit && window_ && !window_->close()) return true;
    return QObject::eventFilter(object, event);
}
void ImageBrowserModule::open() {
    if (!window_) window_ = new ImageBrowserWindow(settings_, loader_);
    if (window_->isMinimized()) window_->showNormal();
    else window_->show();
    window_->raise();
    window_->activateWindow();
}
void ImageBrowserModule::shutdown() {
    if (qApp) qApp->removeEventFilter(this);
    if (window_) { delete window_.data(); window_.clear(); }
}
} // namespace ImageBrowser
