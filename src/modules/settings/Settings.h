#pragma once

#include <QDialog>
#include "ui_Settings.h"

class QKeyEvent;
class QLineEdit;
class QTimer;
class WindowManager;

class Settings : public QDialog
{
	Q_OBJECT

public:
	Settings(WindowManager* windowManager, QWidget *parent = Q_NULLPTR);
	~Settings();
    void readData();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void showStatusTip(const QString& msg, bool ok = true);

private:
    void initTablePath();
    void initThemeSelector();
    void updateScreenshotGlobalKey();
    void updatePinKey();

    void onAutoStartClicked(bool checked);
    void onAutoPin(bool checked);
    void onPinNoBorder(bool checked);
    void onRevertClicked();
    void onTablePathDoubleClicked(int row, int col);
    void onRgbColorToggled(bool checked);
	void onHexColorToggled(bool checked);
    void onAutoSaveChanged();
    void onOpenImagePath();
    void onModifyImagePath();
    void onBackgroundChanged();
    void onBackgroundAlphaReleased();
    void onThemeChanged(int index);
    void onLanguageChanged(int index);
    void onGitHubFieldChanged();
    void onPaddleOcrChanged();

    void initLocalSearchTab();
    void initHttpServerTab();
    void refreshHttpServerState();
    void updateHttpPortWarning();
    void showHttpServerError(const QString& title, const QString& detail);
    void applyModernLayout();
    void loadLocalSearchSettings();

private:
    Q_DISABLE_COPY(Settings)
    Ui::Settings ui;
    WindowManager* windowManager_ = nullptr;
    class QComboBox*   cbTheme_;

    class QListWidget* listLocalSearchRoots_ = nullptr;
    class QPlainTextEdit* teLocalSearchExcludes_ = nullptr;
    class QKeySequenceEdit* kseLocalSearch_ = nullptr;
    class QLabel* labelLocalSearchStatus_ = nullptr;
    class QCheckBox* cbLocalSearchChromeBookmarks_ = nullptr;
    class QCheckBox* cbLocalSearchEdgeBookmarks_ = nullptr;
    class QCheckBox* cbLocalSearchPinyin_ = nullptr;
    class QComboBox* cbLocalSearchWebEngine_ = nullptr;
    class QLineEdit* leHttpDirectory_ = nullptr;
    class QSpinBox* sbHttpPort_ = nullptr;
    class QLineEdit* leHttpBind_ = nullptr;
    class QComboBox* cbHttpProtocol_ = nullptr;
    class QCheckBox* cbHttpCgi_ = nullptr;
    class QPushButton* btnHttpBrowse_ = nullptr;
    class QPushButton* btnHttpOpen_ = nullptr;
    class QPushButton* btnHttpStart_ = nullptr;
    class QPushButton* btnHttpStop_ = nullptr;
    class QLabel* labelHttpStatusDot_ = nullptr;
    class QLabel* labelHttpStatus_ = nullptr;
    class QPlainTextEdit* teHttpAddress_ = nullptr;
    class QLabel* labelHttpPythonNotice_ = nullptr;
    class QLabel* labelHttpPortWarning_ = nullptr;
    QString httpPrimaryUrl_;
    // Probing for python spawns short-lived processes, so the first check is
    // performed on a worker. Start stays disabled until the result arrives.
    bool httpPythonProbed_ = false;
    bool httpPythonReady_ = false;

    QTimer* statusTipTimer_ = nullptr;
};
