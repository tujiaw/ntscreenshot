#pragma once

#include "core/modules/IToolModule.h"
#include <QObject>
#include <QPointer>

class SettingModel;
namespace ImageBrowser {
class ImageLoader;
class ImageBrowserWindow;

class ImageBrowserModule final : public QObject, public IToolModule {
    Q_OBJECT
public:
    explicit ImageBrowserModule(SettingModel* settings);
    QString id() const override { return QStringLiteral("image_browser"); }
    void initialize() override;
    void shutdown() override;
    void open();

protected:
    bool eventFilter(QObject* object, QEvent* event) override;

private:
    SettingModel* settings_;
    ImageLoader* loader_;
    QPointer<ImageBrowserWindow> window_;
};
} // namespace ImageBrowser
