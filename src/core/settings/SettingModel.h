#pragma once

#include <QObject>
#include <QList>
#include <QSettings>
#include <QSize>
#include <QStringList>
#include "core/foundation/Constants.h"
#include "core/theme/AppTheme.h"

struct LlmProviderConfig {
    QString name;
    QString apiBaseUrl;
    QString apiKey;
    QString model;
    double  temperature = 0.7;
};

struct TextSelectionActionConfig {
    QString id;
    QString label;
    QString prompt;
};

struct PaddleOcrConfig {
    bool enabled = false;
    QString jobUrl;
    QString token;
    QString model;
};

struct HttpServerConfig {
    QString directory;
    int port = 8000;
    QString bind = QStringLiteral("0.0.0.0");
    QString protocol = QStringLiteral("HTTP/1.1");
    bool cgi = false;
};

enum class TrayNotificationPosition {
    TopLeft = 0,
    TopRight,
    BottomLeft,
    BottomRight
};

struct WebSearchEngine {
    QString id;
    QString name;
    QString slogan;
    QString baseUrl;
    QString queryParam;
    QString iconPath;
};

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

    void setTextSelectionGlobalKey(const QString &key);
    QString textSelectionGlobalKey() const;

    void setChatGlobalKey(const QString &key);
    QString chatGlobalKey() const;

    // 对话窗口的界面状态：记住“联网 / 浏览器”开关，以及左右两栏的宽度。
    bool chatUseBrowser() const;
    void setChatUseBrowser(bool useBrowser);
    int chatPaneWidth() const;
    void setChatPaneWidth(int width);
    int chatBrowserPaneWidth() const;
    void setChatBrowserPaneWidth(int width);

    void setLocalSearchGlobalKey(const QString& key);
    QString localSearchGlobalKey() const;
    void setLocalSearchRoots(const QStringList& roots);
    QStringList localSearchRoots() const;
    void setLocalSearchExcludePatterns(const QStringList& patterns);
    QStringList localSearchExcludePatterns() const;
    void setLocalSearchBookmarkSources(const QStringList& sources);
    QStringList localSearchBookmarkSources() const;
    QString localSearchWebEngine() const;
    void setLocalSearchWebEngine(const QString& id);
    bool localSearchPinyinEnabled() const;
    void setLocalSearchPinyinEnabled(bool enabled);
    static QVector<WebSearchEngine> localSearchWebEngines();
    static QStringList defaultLocalSearchRoots();
    static QStringList defaultLocalSearchExcludePatterns();

    void setAutoPin(bool enable);
    bool autoPin() const;

    void setPinNoBorder(bool enable);
    bool pinNoBorder() const;

    void setRgbColor(bool enable);
    bool rgbColor() const;

    void setOpenCVMode(bool enable);
    bool openCVMode() const;

    void getAutoSaveImage(bool &autoSave, QString &path) const;
    void setAutoSaveImage(bool autoSave, const QString &path);

    void setBackgroundColor(bool checked, int alpha);
    int backgroundColorAlpha() const;
    bool backgroundColorChecked() const;

    void setThemeMode(AppTheme themeMode);
    AppTheme themeMode() const;

    QList<LlmProviderConfig> llmProviders() const;
    void setLlmProviders(const QList<LlmProviderConfig> &providers);
    int llmActiveProviderIndex() const;
    void setLlmActiveProviderIndex(int index);
    LlmProviderConfig llmActiveProvider() const;

    bool textSelectionEnabled() const;
    void setTextSelectionEnabled(bool enabled);

    QList<TextSelectionActionConfig> textSelectionActions() const;
    void setTextSelectionActions(const QList<TextSelectionActionConfig> &actions);
    bool llmImageTokenSavingEnabled() const;
    void setLlmImageTokenSavingEnabled(bool enabled);

    GitHubImageBedConfig gitHubImageBedConfig() const;
    void setGitHubImageBedConfig(const GitHubImageBedConfig &config);

    PaddleOcrConfig paddleOcrConfig() const;
    void setPaddleOcrConfig(const PaddleOcrConfig &config);

    HttpServerConfig httpServerConfig() const;
    void setHttpServerConfig(const HttpServerConfig &config);

    QSize notificationWindowSize() const;
    void setNotificationWindowSize(const QSize &size);
    TrayNotificationPosition trayNotificationPosition() const;
    void setTrayNotificationPosition(TrayNotificationPosition position);

private:
    Q_DISABLE_COPY(SettingModel)
    QSettings settings_;
};
