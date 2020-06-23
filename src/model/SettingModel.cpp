#include "SettingModel.h"
#include <QApplication>
#include "common/Util.h"

static const QString REG_RUN = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const QString SCREENSHOT_GLOBAL_KEY = "SCREENSHOT_GLOBAL_KEY";
static const QString PIN_KEY = "PIN_KEY";
static const QString UPLOAD_IMAGE_URL_KEY = "UPLOAD_IMAGE_URL_KEY";
static const QString PIN_NO_BORDER = "PIN_NO_BORDER";

static bool defaultAutoStart = false;
static QString defaultScreenshotGlobalKey = "F5";
static QString defaultPinKey = "F6";
static QString defaultUploadImageUrl = "";

SettingModel::SettingModel(QObject *parent)
    : QObject(parent),
    settings_(Util::getConfigPath(), QSettings::IniFormat)
{
    if (settings_.allKeys().isEmpty()) {
        revertDefault();
    }
}

SettingModel::~SettingModel()
{
}

void SettingModel::revertDefault()
{
    setAutoStart(false);
    setScreenshotGlobalKey(defaultScreenshotGlobalKey);
    setPinKey(defaultPinKey);
    settings_.sync();
}

void SettingModel::setAutoStart(bool isAutoStart)
{
    QString appName = QApplication::applicationName();
    QSettings nativeSettings(REG_RUN, QSettings::NativeFormat);
    if (isAutoStart) {
        QString appPath = QApplication::applicationFilePath();
        nativeSettings.setValue(appName, appPath.replace("/", "\\"));
    } else {
        nativeSettings.remove(appName);
    }
}

bool SettingModel::autoStart() const
{
    QString appName = QApplication::applicationName();
    QSettings nativeSettings(REG_RUN, QSettings::NativeFormat);
    QString path = nativeSettings.value(appName).toString();
    return (QApplication::applicationFilePath().replace("/", "\\") == path);
}

void SettingModel::setScreenshotGlobalKey(const QString &key)
{
    settings_.setValue(SCREENSHOT_GLOBAL_KEY, key);
}

QString SettingModel::screenhotGlobalKey() const
{
    return settings_.value(SCREENSHOT_GLOBAL_KEY, "F5").toString();
}

void SettingModel::setPinKey(const QString &key)
{
    settings_.setValue(PIN_KEY, key);
}

QString SettingModel::pinKey() const
{
    return settings_.value(PIN_KEY, "F6").toString();
}

void SettingModel::setUploadImageUrl(const QString &url)
{
    settings_.setValue(UPLOAD_IMAGE_URL_KEY, url);
}

QString SettingModel::uploadImageUrl() const
{
    return settings_.value(UPLOAD_IMAGE_URL_KEY, defaultUploadImageUrl).toString();
}

void SettingModel::setPinNoBorder(bool enable)
{
    settings_.setValue(PIN_NO_BORDER, enable);
}

bool SettingModel::pinNoBorder() const
{
    return settings_.value(PIN_NO_BORDER, false).toBool();
}
