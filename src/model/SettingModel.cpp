#include "SettingModel.h"
#include <QApplication>
#include "common/Util.h"

static const QString REG_RUN = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const QString SCREENSHOT_GLOBAL_KEY = "SCREENSHOT_GLOBAL_KEY";
static const QString PIN_GLOBAL_KEY = "PIN_GLOBAL_KEY";
static const QString UPLOAD_IMAGE_URL_KEY = "UPLOAD_IMAGE_URL_KEY";
static const QString PIN_NO_BORDER = "PIN_NO_BORDER";
static const QString RGB_COLOR = "RGB_COLOR";
static const QString AUTO_SAVE = "AUTO_SAVE";
static const QString AUTO_SAVE_PATH = "AUTO_SAVE_PATH";

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
    setPinGlobalKey(defaultPinKey);
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

void SettingModel::setPinGlobalKey(const QString &key)
{
    settings_.setValue(PIN_GLOBAL_KEY, key);
}

QString SettingModel::pinGlobalKey() const
{
    return settings_.value(PIN_GLOBAL_KEY, "F6").toString();
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

void SettingModel::setRgbColor(bool enable)
{
    settings_.setValue(RGB_COLOR, enable);
}

bool SettingModel::rgbColor() const
{
    return settings_.value(RGB_COLOR, true).toBool();
}

void SettingModel::getAutoSaveImage(bool &autoSave, QString &path)
{
    QDir dir(QDir::homePath());
    if (!dir.cd("Pictures")) {
        dir.mkdir("ntscreenshot");
        dir.cd("ntscreenshot");
    }
    autoSave = settings_.value(AUTO_SAVE, false).toBool();
    path = settings_.value(AUTO_SAVE_PATH, dir.absolutePath()).toString();
}

void SettingModel::setAutoSaveImage(bool autoSave, const QString &path)
{
    settings_.setValue(AUTO_SAVE, autoSave);
    settings_.setValue(AUTO_SAVE_PATH, path);
}
