#include "Settings.h"
#include "SettingsInternal.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QKeySequence>
#include <QKeySequenceEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QStandardPaths>

#include "app/WindowManager.h"
#include "core/platform/Util.h"
#include "core/settings/SettingModel.h"
#include "core/theme/DarkStyle.h"

void Settings::updateScreenshotGlobalKey()
{
    const QString key = settings_detail::keySequenceText(ui.kseScreenshot->keySequence());
    if (!key.isEmpty() && windowManager_->setScreenshotGlobalKey(key)) {
        windowManager_->setting()->setScreenshotGlobalKey(key);
        settings_detail::setStatusIcon(ui.pbScreenshotStatus, true);
        emit windowManager_->sigSettingChanged();
        showStatusTip(QCoreApplication::translate("App", "截图快捷键已更新"));
    } else {
        settings_detail::setStatusIcon(ui.pbScreenshotStatus, false);
        if (key.isEmpty()) {
            showStatusTip(QCoreApplication::translate("App", "快捷键不能为空"), false);
        } else {
            const QString reason = windowManager_->lastHotkeyError();
            showStatusTip(reason.isEmpty()
                              ? QCoreApplication::translate("App", "快捷键注册失败，可能被占用")
                              : QCoreApplication::translate("App", "快捷键注册失败：%1").arg(reason),
                          false);
        }
    }
}

void Settings::updatePinKey()
{
    const QString key = settings_detail::keySequenceText(ui.ksePin->keySequence());
    if (!key.isEmpty() && windowManager_->setPinGlobalKey(key)) {
        windowManager_->setting()->setPinGlobalKey(key);
        settings_detail::setStatusIcon(ui.pbPinStatus, true);
        emit windowManager_->sigSettingChanged();
        showStatusTip(QCoreApplication::translate("App", "贴图快捷键已更新"));
    } else {
        settings_detail::setStatusIcon(ui.pbPinStatus, false);
        if (key.isEmpty()) {
            showStatusTip(QCoreApplication::translate("App", "快捷键不能为空"), false);
        } else {
            const QString reason = windowManager_->lastHotkeyError();
            showStatusTip(reason.isEmpty()
                              ? QCoreApplication::translate("App", "快捷键注册失败，可能被占用")
                              : QCoreApplication::translate("App", "快捷键注册失败：%1").arg(reason),
                          false);
        }
    }
}

void Settings::onAutoStartClicked(bool checked)
{
    windowManager_->setting()->setAutoStart(checked);
    emit windowManager_->sigSettingChanged();
    showStatusTip(checked ? QCoreApplication::translate("App", "已开启开机自启动") : QCoreApplication::translate("App", "已关闭开机自启动"));
}

void Settings::onAutoPin(bool checked)
{
    windowManager_->setting()->setAutoPin(checked);
    emit windowManager_->sigSettingChanged();
    showStatusTip(checked ? QCoreApplication::translate("App", "已开启自动贴图") : QCoreApplication::translate("App", "已关闭自动贴图"));
}

void Settings::onPinNoBorder(bool checked)
{
    windowManager_->setting()->setPinNoBorder(checked);
    emit windowManager_->sigSettingChanged();
    showStatusTip(checked ? QCoreApplication::translate("App", "已开启贴图无边框") : QCoreApplication::translate("App", "已关闭贴图无边框"));
}

void Settings::onRevertClicked()
{
    windowManager_->setting()->revertDefault();
    readData();
    CDarkStyle::assign(windowManager_->setting()->themeMode());
    emit windowManager_->sigSettingChanged();
    showStatusTip(QCoreApplication::translate("App", "已恢复默认设置"));
}


void Settings::onAutoSaveChanged()
{
    bool autoSave = ui.cbAutoSave->isChecked();
    ui.pbModifyImagePath->setEnabled(autoSave);
    windowManager_->setting()->setAutoSaveImage(autoSave, ui.leSavePath->text().trimmed());
    showStatusTip(autoSave ? QCoreApplication::translate("App", "已开启自动保存") : QCoreApplication::translate("App", "已关闭自动保存"));
}

void Settings::onOpenImagePath()
{
	Util::shellExecute(ui.leSavePath->text().trimmed());
}

void Settings::onModifyImagePath()
{
	QString newDir = QFileDialog::getExistingDirectory(nullptr, QCoreApplication::translate("App", "选择截图保存目录"));
	if (!newDir.isEmpty()) {
		ui.leSavePath->setText(newDir);
		onAutoSaveChanged();
		showStatusTip(QCoreApplication::translate("App", "保存路径已更新"));
	}
}


void Settings::onLanguageChanged(int index)
{
    windowManager_->setting()->setUiLanguage(index == 1 ? QStringLiteral("zh")
                                                        : QStringLiteral("en"));
    showStatusTip(tr("语言已保存，重启应用后生效"));
}


void Settings::onGitHubFieldChanged()
{
    GitHubImageBedConfig config;
    config.owner      = ui.leGitHubOwner->text().trimmed();
    config.repo       = ui.leGitHubRepo->text().trimmed();
    config.branch     = ui.leGitHubBranch->text().trimmed();
    config.token      = ui.leGitHubToken->text().trimmed();
    config.pathPrefix = ui.leGitHubPathPrefix->text().trimmed();
    config.cdnUrl     = ui.leGitHubCdnUrl->text().trimmed();
    windowManager_->setting()->setGitHubImageBedConfig(config);
}

void Settings::onPaddleOcrChanged()
{
    PaddleOcrConfig config;
    config.enabled = ui.cbPaddleOcrEnabled->isChecked();
    config.jobUrl = ui.lePaddleOcrUrl->text().trimmed();
    config.token = ui.lePaddleOcrToken->text().trimmed();
    config.model = ui.lePaddleOcrModel->text().trimmed();
    windowManager_->setting()->setPaddleOcrConfig(config);
    ui.lePaddleOcrUrl->setEnabled(config.enabled);
    ui.lePaddleOcrToken->setEnabled(config.enabled);
    ui.lePaddleOcrModel->setEnabled(config.enabled);
}
