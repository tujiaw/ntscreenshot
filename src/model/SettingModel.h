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

    void setPinGlobalKey(const QString &key);
    QString pinGlobalKey() const;

    void setUploadImageUrl(const QString &url);
    QString uploadImageUrl() const;

    void setPinNoBorder(bool enable);
    bool pinNoBorder() const;

    void setRgbColor(bool enable);
    bool rgbColor() const;

    void getAutoSaveImage(bool &autoSave, QString &path) const;
    void setAutoSaveImage(bool autoSave, const QString &path);

private:
    Q_DISABLE_COPY(SettingModel)
    QSettings settings_;
};
