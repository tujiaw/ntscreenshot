#pragma once

#include <QObject>
#include <QSettings>

class SettingModel : public QObject
{
    Q_OBJECT

public:
    SettingModel(QObject *parent);
    ~SettingModel();

    void revertDefault();

    void setAutoStart(bool isAutoStart);
    bool autoStart() const;

    void setScreenshotGlobalKey(const QString &key);
    QString screenhotGlobalKey() const;

    void setPinKey(const QString &key);
    QString pinKey() const;

    void setUploadImageUrl(const QString &url);
    QString uploadImageUrl() const;

    void setPinNoBorder(bool enable);
    bool pinNoBorder() const;

private:
    Q_DISABLE_COPY(SettingModel)
    QSettings settings_;
};
