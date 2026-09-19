#include "SettingModel.h"
#include <QApplication>
#include <QUuid>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QStandardPaths>
#include "core/platform/Util.h"

#ifdef Q_OS_WIN
#include <shlobj.h>

static const QString REG_RUN = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
#endif

// [General] 节 —— 通用设置
static const QString KEY_SCREENSHOT     = "SCREENSHOT_GLOBAL_KEY";
static const QString KEY_PIN            = "PIN_GLOBAL_KEY";
static const QString KEY_TEXT_SELECTION = "TEXT_SELECTION_GLOBAL_KEY";
static const QString KEY_CHAT           = "CHAT_GLOBAL_KEY";
// [Chat] 节 —— 对话窗口的界面状态
static const QString KEY_CHAT_USE_BROWSER = "CHAT/USE_BROWSER";
static const QString KEY_CHAT_PANE_WIDTH  = "CHAT/PANE_WIDTH";
static const QString KEY_CHAT_BROWSER_WIDTH = "CHAT/BROWSER_WIDTH";
static const QString KEY_CHAT_TOOL_CALL_LIMIT = "CHAT/TOOL_CALL_LIMIT";
static const QString KEY_LOCAL_SEARCH   = "LOCAL_SEARCH/GLOBAL_KEY";
static const QString KEY_LOCAL_SEARCH_DEFAULT_VERSION = "LOCAL_SEARCH/HOTKEY_DEFAULT_VERSION";
static const QString KEY_LOCAL_SEARCH_ROOTS = "LOCAL_SEARCH/ROOTS";
static const QString KEY_LOCAL_SEARCH_ROOTS_DEFAULT_VERSION =
    "LOCAL_SEARCH/ROOTS_DEFAULT_VERSION";
static const QString KEY_LOCAL_SEARCH_IGNORE_PATTERNS = "LOCAL_SEARCH/IGNORE_PATTERNS";
static const QString KEY_LOCAL_SEARCH_BOOKMARK_SOURCES = "LOCAL_SEARCH/BOOKMARK_SOURCES";
static const QString KEY_LOCAL_SEARCH_WEB_ENGINE = "LOCAL_SEARCH/WEB_ENGINE";
static const QString KEY_LOCAL_SEARCH_PINYIN = "LOCAL_SEARCH/PINYIN_ENABLED";
static const QString KEY_LOCAL_SEARCH_EXCLUDES_DEFAULT_VERSION =
    "LOCAL_SEARCH/EXCLUDE_DEFAULT_VERSION";
static const QString KEY_TEXT_SELECTION_ENABLED = "TEXT_SELECTION_ENABLED";
static const QString KEY_AUTO_PIN       = "AUTO_PIN";
static const QString KEY_PIN_NO_BORDER  = "PIN_NO_BORDER";
static const QString KEY_RGB_COLOR      = "RGB_COLOR";
static const QString KEY_OPENCV_MODE    = "OPENCV_MODE";
static const QString KEY_AUTO_SAVE      = "AUTO_SAVE";
static const QString KEY_AUTO_SAVE_PATH = "AUTO_SAVE_PATH";
static const QString KEY_BG_ALPHA       = "BACKGROUND_COLOR_ALPHA";
static const QString KEY_BG_CHECKED     = "BACKGROUND_COLOR_CHECKED";
static const QString KEY_THEME_MODE     = "THEME_MODE";
static const QString KEY_NOTIFY_WIDTH   = "NOTIFY_WIDTH";
static const QString KEY_NOTIFY_HEIGHT  = "NOTIFY_HEIGHT";
static const QString KEY_NOTIFY_POS     = "NOTIFY_POSITION";

// [GitHub] 节 —— GitHub 图床配置
static const QString KEY_GITHUB_OWNER       = "GitHub/OWNER";
static const QString KEY_GITHUB_REPO        = "GitHub/REPO";
static const QString KEY_GITHUB_BRANCH      = "GitHub/BRANCH";
static const QString KEY_GITHUB_TOKEN       = "GitHub/TOKEN";
static const QString KEY_GITHUB_PATH_PREFIX = "GitHub/PATH_PREFIX";
static const QString KEY_GITHUB_CDN_URL     = "GitHub/CDN_URL";

// [OCR] section - PaddleOCR hosted service configuration
static const QString KEY_OCR_ENABLED = "OCR/ENABLED";
static const QString KEY_OCR_JOB_URL = "OCR/JOB_URL";
static const QString KEY_OCR_TOKEN   = "OCR/TOKEN";
static const QString KEY_OCR_MODEL   = "OCR/MODEL";
static const QString DEFAULT_OCR_JOB_URL =
    QStringLiteral("https://paddleocr.aistudio-app.com/api/v2/ocr/jobs");
static const QString DEFAULT_OCR_MODEL = QStringLiteral("PP-OCRv6");

// [HttpServer] section - background python -m http.server configuration
static const QString KEY_HTTP_SERVER_DIR      = "HttpServer/DIRECTORY";
static const QString KEY_HTTP_SERVER_PORT     = "HttpServer/PORT";
static const QString KEY_HTTP_SERVER_BIND     = "HttpServer/BIND";
static const QString KEY_HTTP_SERVER_PROTOCOL = "HttpServer/PROTOCOL";
static const QString KEY_HTTP_SERVER_CGI      = "HttpServer/CGI";

// [LLM] 节 —— LLM 全局设置
static const QString KEY_LLM_ACTIVE_PROVIDER = "LLM/ACTIVE_PROVIDER";
static const QString KEY_LLM_PROVIDERS_COUNT = "LLM/PROVIDERS_COUNT";

static const QString KEY_TEXT_SELECTION_ACTIONS_COUNT = "LLM/TextSelectionActions/COUNT";
static const QString KEY_LLM_IMAGE_TOKEN_SAVING = "LLM/IMAGE_TOKEN_SAVING";

static const double DEFAULT_LLM_TEMPERATURE = 0.7;
static const QSize  DEFAULT_NOTIFY_SIZE(420, 300);
static constexpr int DEFAULT_CHAT_TOOL_CALL_LIMIT = 10;
static constexpr int LOCAL_SEARCH_HOTKEY_DEFAULT_VERSION = 2;
static constexpr int LOCAL_SEARCH_ROOTS_DEFAULT_VERSION = 1;
static constexpr int LOCAL_SEARCH_EXCLUDES_DEFAULT_VERSION = 2;

// [LLM/Provider/N] 节 key 路径
static QString providerKey(int index, const char *field)
{
    return QString("LLM/Provider/%1/%2").arg(index).arg(field);
}

static QString textSelectionActionKey(int index, const char *field)
{
    return QString("LLM/TextSelectionActions/%1/%2").arg(index).arg(field);
}

static QList<TextSelectionActionConfig> defaultTextSelectionActions()
{
    return {
        { QStringLiteral("ask_ai"),    QStringLiteral("AI 搜索"), QStringLiteral("请基于以上文本进行搜索和解释。") },
        { QStringLiteral("explain"),   QStringLiteral("解释"),     QStringLiteral("请解释以上文本。") },
        { QStringLiteral("translate"), QStringLiteral("翻译"),     QStringLiteral("请翻译以上文本，并给出简短说明。") }
    };
}

// --------------------------------------------------------------------------

SettingModel::SettingModel(QObject *parent)
    : QObject(parent),
    settings_(Util::getConfigPath(), QSettings::IniFormat)
{
    if (settings_.allKeys().isEmpty()) {
        revertDefault();
    }
    if (settings_.value(KEY_LOCAL_SEARCH_DEFAULT_VERSION, 0).toInt()
        < LOCAL_SEARCH_HOTKEY_DEFAULT_VERSION) {
        // Migrate installations that still use the previous built-in default.
        // Other user-selected shortcuts remain untouched.
        if (settings_.value(KEY_LOCAL_SEARCH).toString()
            .compare(QStringLiteral("Ctrl+Alt+Space"), Qt::CaseInsensitive) == 0) {
            setLocalSearchGlobalKey(QStringLiteral("Alt+Space"));
        }
        settings_.setValue(KEY_LOCAL_SEARCH_DEFAULT_VERSION,
                           LOCAL_SEARCH_HOTKEY_DEFAULT_VERSION);
        settings_.sync();
    }
    if (settings_.value(KEY_LOCAL_SEARCH_ROOTS_DEFAULT_VERSION, 0).toInt()
        < LOCAL_SEARCH_ROOTS_DEFAULT_VERSION) {
        // Expand only the untouched built-in defaults. An explicitly stored
        // root list, including an intentionally empty list, remains unchanged.
        if (!settings_.contains(KEY_LOCAL_SEARCH_ROOTS)) {
            settings_.setValue(KEY_LOCAL_SEARCH_ROOTS, defaultLocalSearchRoots());
        }
        settings_.setValue(KEY_LOCAL_SEARCH_ROOTS_DEFAULT_VERSION,
                           LOCAL_SEARCH_ROOTS_DEFAULT_VERSION);
        settings_.sync();
    }
    if (settings_.value(KEY_LOCAL_SEARCH_EXCLUDES_DEFAULT_VERSION, 0).toInt()
        < LOCAL_SEARCH_EXCLUDES_DEFAULT_VERSION) {
        // Regex rules from version 1 are intentionally retained under their
        // legacy key. The gitignore-style syntax uses a separate key so an old
        // expression can never be interpreted as a broad glob by mistake.
        if (!settings_.contains(KEY_LOCAL_SEARCH_IGNORE_PATTERNS)) {
            setLocalSearchExcludePatterns(defaultLocalSearchExcludePatterns());
        }
        settings_.setValue(KEY_LOCAL_SEARCH_EXCLUDES_DEFAULT_VERSION,
                           LOCAL_SEARCH_EXCLUDES_DEFAULT_VERSION);
        settings_.sync();
    }
}

SettingModel::~SettingModel() {}

void SettingModel::revertDefault()
{
    setAutoStart(false);
    setScreenshotGlobalKey("F5");
    setPinGlobalKey("F6");
    setTextSelectionGlobalKey(QString());
    setChatGlobalKey(QString());
    setLocalSearchGlobalKey(QStringLiteral("Alt+Space"));
    settings_.setValue(KEY_LOCAL_SEARCH_DEFAULT_VERSION,
                       LOCAL_SEARCH_HOTKEY_DEFAULT_VERSION);
    settings_.remove(KEY_LOCAL_SEARCH_ROOTS);
    settings_.setValue(KEY_LOCAL_SEARCH_ROOTS_DEFAULT_VERSION,
                       LOCAL_SEARCH_ROOTS_DEFAULT_VERSION);
    setLocalSearchExcludePatterns(defaultLocalSearchExcludePatterns());
    settings_.setValue(KEY_LOCAL_SEARCH_EXCLUDES_DEFAULT_VERSION,
                       LOCAL_SEARCH_EXCLUDES_DEFAULT_VERSION);
    setThemeMode(AppTheme::Dark);
    setNotificationWindowSize(DEFAULT_NOTIFY_SIZE);
    setTrayNotificationPosition(TrayNotificationPosition::BottomRight);
    setChatToolCallLimit(DEFAULT_CHAT_TOOL_CALL_LIMIT);

    LlmProviderConfig defaultProvider;
    defaultProvider.name        = QStringLiteral("Default");
    defaultProvider.temperature = DEFAULT_LLM_TEMPERATURE;
    setLlmProviders({defaultProvider});
    setLlmActiveProviderIndex(0);
    setTextSelectionActions(defaultTextSelectionActions());
    setLlmImageTokenSavingEnabled(true);
    setPaddleOcrConfig({false, DEFAULT_OCR_JOB_URL, QString(), DEFAULT_OCR_MODEL});
    settings_.sync();
}

// --------------------------------------------------------------------------

void SettingModel::setAutoStart(bool isAutoStart)
{
#ifdef Q_OS_WIN
    QString appName = QApplication::applicationName();
    if (appName.isEmpty()) {
        appName = QStringLiteral("ntscreenshot");
    }
    QSettings nativeSettings(REG_RUN, QSettings::NativeFormat);
    if (isAutoStart) {
        QString appPath = QApplication::applicationFilePath();
        nativeSettings.setValue(appName, appPath.replace("/", "\\"));
    } else {
        nativeSettings.remove(appName);
    }
#elif defined(Q_OS_LINUX)
    const QString dirPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/autostart/");
    QDir().mkpath(dirPath);
    const QString desktopPath = dirPath + QStringLiteral("ntscreenshot.desktop");
    if (isAutoStart) {
        QFile f(desktopPath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&f);
            ts << "[Desktop Entry]\n"
               << "Type=Application\n"
               << "Name=ntscreenshot\n"
               << "Exec=" << QApplication::applicationFilePath() << " \n"
               << "Hidden=false\n"
               << "NoDisplay=false\n"
               << "X-GNOME-Autostart-enabled=true\n";
        }
    } else {
        QFile::remove(desktopPath);
    }
#else
    Q_UNUSED(isAutoStart);
#endif
}

bool SettingModel::autoStart() const
{
#ifdef Q_OS_WIN
    QString appName = QApplication::applicationName();
    if (appName.isEmpty()) {
        appName = QStringLiteral("ntscreenshot");
    }
    QSettings nativeSettings(REG_RUN, QSettings::NativeFormat);
    QString path = nativeSettings.value(appName).toString();
    return (QApplication::applicationFilePath().replace("/", "\\") == path);
#elif defined(Q_OS_LINUX)
    const QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/autostart/ntscreenshot.desktop");
    return QFile::exists(desktopPath);
#else
    return false;
#endif
}

void SettingModel::setScreenshotGlobalKey(const QString &key)
{
    settings_.setValue(KEY_SCREENSHOT, key);
}

QString SettingModel::screenhotGlobalKey() const
{
    return settings_.value(KEY_SCREENSHOT, "F5").toString();
}

void SettingModel::setPinGlobalKey(const QString &key)
{
    settings_.setValue(KEY_PIN, key);
}

QString SettingModel::pinGlobalKey() const
{
    return settings_.value(KEY_PIN, "F6").toString();
}

void SettingModel::setTextSelectionGlobalKey(const QString &key)
{
    settings_.setValue(KEY_TEXT_SELECTION, key);
}

QString SettingModel::textSelectionGlobalKey() const
{
    return settings_.value(KEY_TEXT_SELECTION, QString()).toString();
}

void SettingModel::setChatGlobalKey(const QString &key)
{
    settings_.setValue(KEY_CHAT, key);
}

QString SettingModel::chatGlobalKey() const
{
    return settings_.value(KEY_CHAT, QString()).toString();
}

// 对话窗口上次选的是“浏览器”（true）还是“联网”（false）。两者互斥，一个 bool 就够。
bool SettingModel::chatUseBrowser() const
{
    return settings_.value(KEY_CHAT_USE_BROWSER, false).toBool();
}

void SettingModel::setChatUseBrowser(bool useBrowser)
{
    settings_.setValue(KEY_CHAT_USE_BROWSER, useBrowser);
}

// 左栏（对话）宽度。返回 0 表示还没存过，调用方按当前布局决定。
int SettingModel::chatPaneWidth() const
{
    return qMax(0, settings_.value(KEY_CHAT_PANE_WIDTH, 0).toInt());
}

void SettingModel::setChatPaneWidth(int width)
{
    if (width > 0) settings_.setValue(KEY_CHAT_PANE_WIDTH, width);
}

// 右栏（浏览器）宽度。返回 0 表示还没存过，调用方回退到默认宽度。
int SettingModel::chatBrowserPaneWidth() const
{
    return qMax(0, settings_.value(KEY_CHAT_BROWSER_WIDTH, 0).toInt());
}

void SettingModel::setChatBrowserPaneWidth(int width)
{
    if (width > 0) settings_.setValue(KEY_CHAT_BROWSER_WIDTH, width);
}

int SettingModel::chatToolCallLimit() const
{
    return qBound(1,
                  settings_.value(KEY_CHAT_TOOL_CALL_LIMIT,
                                  DEFAULT_CHAT_TOOL_CALL_LIMIT).toInt(),
                  100);
}

void SettingModel::setChatToolCallLimit(int limit)
{
    settings_.setValue(KEY_CHAT_TOOL_CALL_LIMIT, qBound(1, limit, 100));
}

void SettingModel::setLocalSearchGlobalKey(const QString& key)
{
    settings_.setValue(KEY_LOCAL_SEARCH, key);
    settings_.sync();
}

QString SettingModel::localSearchGlobalKey() const
{
    return settings_.value(KEY_LOCAL_SEARCH, QStringLiteral("Alt+Space")).toString();
}

void SettingModel::setLocalSearchRoots(const QStringList& roots)
{
    settings_.setValue(KEY_LOCAL_SEARCH_ROOTS, roots);
}

QStringList SettingModel::localSearchRoots() const
{
    if (settings_.contains(KEY_LOCAL_SEARCH_ROOTS)) {
        return settings_.value(KEY_LOCAL_SEARCH_ROOTS).toStringList();
    }
    return defaultLocalSearchRoots();
}

QStringList SettingModel::defaultLocalSearchRoots()
{
    QStringList roots;
    const auto addPath = [&roots](const QString& value) {
        const QString trimmed = value.trimmed();
        if (trimmed.isEmpty()) return;
        const QString path = QDir::fromNativeSeparators(QDir::cleanPath(trimmed));
        if (!QFileInfo(path).isDir()) return;

        const QString pathPrefix = path.endsWith(u'/') ? path : path + u'/';
        for (qsizetype i = roots.size(); i > 0; --i) {
            const qsizetype index = i - 1;
            const QString existing = roots.at(index);
            const QString existingPrefix = existing.endsWith(u'/') ? existing : existing + u'/';
            if (path.compare(existing, Qt::CaseInsensitive) == 0) return;
            if (path.startsWith(existingPrefix, Qt::CaseInsensitive)) return;
            if (existing.startsWith(pathPrefix, Qt::CaseInsensitive)) roots.removeAt(index);
        }
        roots.push_back(path);
    };
    const auto addLocation = [&addPath](QStandardPaths::StandardLocation location) {
        addPath(QStandardPaths::writableLocation(location));
    };

    addLocation(QStandardPaths::DesktopLocation);
    addLocation(QStandardPaths::DocumentsLocation);
    addLocation(QStandardPaths::DownloadLocation);
    addLocation(QStandardPaths::PicturesLocation);
    addLocation(QStandardPaths::MusicLocation);
    addLocation(QStandardPaths::MoviesLocation);

#ifdef Q_OS_WIN
    PWSTR oneDrivePath = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_SkyDrive, KF_FLAG_DEFAULT,
                                       nullptr, &oneDrivePath))) {
        addPath(QString::fromWCharArray(oneDrivePath));
    }
    CoTaskMemFree(oneDrivePath);
#endif
    return roots;
}

void SettingModel::setLocalSearchExcludePatterns(const QStringList& patterns)
{
    settings_.setValue(KEY_LOCAL_SEARCH_IGNORE_PATTERNS, patterns);
}

void SettingModel::setLocalSearchBookmarkSources(const QStringList& sources)
{
    QStringList normalized;
    for (const QString& source : sources) {
        const QString value = source.trimmed().toLower();
        if ((value == QStringLiteral("chrome") || value == QStringLiteral("edge"))
            && !normalized.contains(value)) {
            normalized.push_back(value);
        }
    }
    settings_.setValue(KEY_LOCAL_SEARCH_BOOKMARK_SOURCES, normalized);
    settings_.sync();
}

QStringList SettingModel::localSearchBookmarkSources() const
{
    return settings_.value(KEY_LOCAL_SEARCH_BOOKMARK_SOURCES).toStringList();
}

QString SettingModel::localSearchWebEngine() const
{
    return settings_.value(KEY_LOCAL_SEARCH_WEB_ENGINE, QStringLiteral("bing")).toString();
}

void SettingModel::setLocalSearchWebEngine(const QString& id)
{
    settings_.setValue(KEY_LOCAL_SEARCH_WEB_ENGINE, id);
    settings_.sync();
}

bool SettingModel::localSearchPinyinEnabled() const
{
    return settings_.value(KEY_LOCAL_SEARCH_PINYIN, true).toBool();
}

void SettingModel::setLocalSearchPinyinEnabled(bool enabled)
{
    settings_.setValue(KEY_LOCAL_SEARCH_PINYIN, enabled);
    settings_.sync();
}

QVector<WebSearchEngine> SettingModel::localSearchWebEngines()
{
    return {
        {QStringLiteral("bing"), QStringLiteral("Bing"), QStringLiteral("微软必应搜索"),
         QStringLiteral("https://www.bing.com/search"), QStringLiteral("q"),
         QStringLiteral(":/icons/bing.ico")},
        {QStringLiteral("google"), QStringLiteral("Google"), QStringLiteral("全球最大的搜索引擎"),
         QStringLiteral("https://www.google.com/search"), QStringLiteral("q"),
         QStringLiteral(":/icons/google.ico")},
        {QStringLiteral("baidu"), QStringLiteral("百度"), QStringLiteral("百度一下，你就知道"),
         QStringLiteral("https://www.baidu.com/s"), QStringLiteral("wd"),
         QStringLiteral(":/icons/baidu.ico")},
        {QStringLiteral("duckduckgo"), QStringLiteral("DuckDuckGo"), QStringLiteral("保护隐私的搜索引擎"),
         QStringLiteral("https://duckduckgo.com/"), QStringLiteral("q"),
         QStringLiteral(":/icons/duck.ico")},
        {QStringLiteral("perplexity"), QStringLiteral("Perplexity"), QStringLiteral("AI 驱动的答案引擎"),
         QStringLiteral("https://www.perplexity.ai/search"), QStringLiteral("q"),
         QStringLiteral(":/icons/perplexity.ico")},
        {QStringLiteral("metaso"), QStringLiteral("秘塔AI搜索"), QStringLiteral("没有广告，直达结果"),
         QStringLiteral("https://metaso.cn/"), QStringLiteral("q"),
         QStringLiteral(":/icons/metaso.ico")},
        {QStringLiteral("tiangong"), QStringLiteral("天工AI搜索"), QStringLiteral("新一代 AI 搜索"),
         QStringLiteral("https://www.tiangong.cn/"), QStringLiteral("q"),
         QStringLiteral(":/icons/tiangong.ico")},
    };
}

QStringList SettingModel::localSearchExcludePatterns() const
{
    if (!settings_.contains(KEY_LOCAL_SEARCH_IGNORE_PATTERNS)) {
        return defaultLocalSearchExcludePatterns();
    }
    return settings_.value(KEY_LOCAL_SEARCH_IGNORE_PATTERNS).toStringList();
}

QStringList SettingModel::defaultLocalSearchExcludePatterns()
{
    return {
        QStringLiteral("# 版本控制与开发缓存"),
        QStringLiteral(".git/"),
        QStringLiteral(".svn/"),
        QStringLiteral(".hg/"),
        QStringLiteral("node_modules/"),
        QStringLiteral("__pycache__/"),
        QStringLiteral(".venv/"),
        QStringLiteral("venv/"),
        QStringLiteral("build/"),
        QStringLiteral("cmake-build-*/"),
        QStringLiteral(".vs/"),
        QStringLiteral(""),
        QStringLiteral("# Windows 系统目录"),
        QStringLiteral("$Recycle.Bin/"),
        QStringLiteral("System Volume Information/"),
        QStringLiteral(""),
        QStringLiteral("# 临时文件与未完成下载"),
        QStringLiteral("*~"),
        QStringLiteral("*.tmp"),
        QStringLiteral("*.temp"),
        QStringLiteral("*.part"),
        QStringLiteral("*.crdownload")
    };
}

bool SettingModel::textSelectionEnabled() const
{
    return settings_.value(KEY_TEXT_SELECTION_ENABLED, true).toBool();
}

void SettingModel::setTextSelectionEnabled(bool enabled)
{
    settings_.setValue(KEY_TEXT_SELECTION_ENABLED, enabled);
    settings_.sync();
}

void SettingModel::setAutoPin(bool enable)
{
    settings_.setValue(KEY_AUTO_PIN, enable);
}

bool SettingModel::autoPin() const
{
    return settings_.value(KEY_AUTO_PIN, false).toBool();
}

void SettingModel::setPinNoBorder(bool enable)
{
    settings_.setValue(KEY_PIN_NO_BORDER, enable);
}

bool SettingModel::pinNoBorder() const
{
    return settings_.value(KEY_PIN_NO_BORDER, false).toBool();
}

void SettingModel::setRgbColor(bool enable)
{
    settings_.setValue(KEY_RGB_COLOR, enable);
}

bool SettingModel::rgbColor() const
{
    return settings_.value(KEY_RGB_COLOR, true).toBool();
}

void SettingModel::setOpenCVMode(bool enable)
{
    settings_.setValue(KEY_OPENCV_MODE, enable);
}

bool SettingModel::openCVMode() const
{
    return settings_.value(KEY_OPENCV_MODE, false).toBool();
}

void SettingModel::getAutoSaveImage(bool &autoSave, QString &path) const
{
    QDir dir = QDir::homePath();
    if (!dir.cd("Pictures")) {
        dir.mkdir("ntscreenshot");
        dir.cd("ntscreenshot");
    }
    autoSave = settings_.value(KEY_AUTO_SAVE, false).toBool();
    path     = settings_.value(KEY_AUTO_SAVE_PATH, dir.absolutePath()).toString();
}

void SettingModel::setAutoSaveImage(bool autoSave, const QString &path)
{
    settings_.setValue(KEY_AUTO_SAVE, autoSave);
    settings_.setValue(KEY_AUTO_SAVE_PATH, path);
}

void SettingModel::setBackgroundColor(bool checked, int alpha)
{
    settings_.setValue(KEY_BG_CHECKED, checked);
    settings_.setValue(KEY_BG_ALPHA, alpha);
}

int SettingModel::backgroundColorAlpha() const
{
    return settings_.value(KEY_BG_ALPHA, 160).toInt();
}

bool SettingModel::backgroundColorChecked() const
{
    return settings_.value(KEY_BG_CHECKED, false).toBool();
}

void SettingModel::setThemeMode(AppTheme themeMode)
{
    settings_.setValue(KEY_THEME_MODE, appThemeToSettingValue(themeMode));
    settings_.sync();
}

AppTheme SettingModel::themeMode() const
{
    return appThemeFromSettingValue(settings_.value(KEY_THEME_MODE, QStringLiteral("dark")).toString());
}

// --------------------------------------------------------------------------

QList<LlmProviderConfig> SettingModel::llmProviders() const
{
    int count = settings_.value(KEY_LLM_PROVIDERS_COUNT, 0).toInt();
    if (count == 0) {
        return {};
    }

    QList<LlmProviderConfig> providers;
    providers.reserve(count);
    for (int i = 0; i < count; ++i) {
        LlmProviderConfig p;
        p.name        = settings_.value(providerKey(i, "NAME")).toString();
        p.apiBaseUrl  = settings_.value(providerKey(i, "BASE_URL")).toString();
        p.apiKey      = settings_.value(providerKey(i, "API_KEY")).toString();
        p.model       = settings_.value(providerKey(i, "MODEL")).toString();
        p.temperature = settings_.value(providerKey(i, "TEMPERATURE"), DEFAULT_LLM_TEMPERATURE).toDouble();
        providers.append(p);
    }
    return providers;
}

void SettingModel::setLlmProviders(const QList<LlmProviderConfig> &providers)
{
    // 清理多余的旧 provider 节
    int oldCount = settings_.value(KEY_LLM_PROVIDERS_COUNT, 0).toInt();
    for (int i = providers.size(); i < oldCount; ++i) {
        settings_.remove(QString("LLM/Provider/%1").arg(i));
    }

    settings_.setValue(KEY_LLM_PROVIDERS_COUNT, providers.size());
    for (int i = 0; i < providers.size(); ++i) {
        const LlmProviderConfig &p = providers.at(i);
        settings_.setValue(providerKey(i, "NAME"),        p.name);
        settings_.setValue(providerKey(i, "BASE_URL"),    p.apiBaseUrl);
        settings_.setValue(providerKey(i, "API_KEY"),     p.apiKey);
        settings_.setValue(providerKey(i, "MODEL"),       p.model);
        settings_.setValue(providerKey(i, "TEMPERATURE"), p.temperature);
    }
    settings_.sync();
}

int SettingModel::llmActiveProviderIndex() const
{
    return settings_.value(KEY_LLM_ACTIVE_PROVIDER, 0).toInt();
}

void SettingModel::setLlmActiveProviderIndex(int index)
{
    settings_.setValue(KEY_LLM_ACTIVE_PROVIDER, index);
    settings_.sync();
}

LlmProviderConfig SettingModel::llmActiveProvider() const
{
    const auto providers = llmProviders();
    if (providers.isEmpty()) {
        return LlmProviderConfig{};
    }
    const int index = qBound(0, llmActiveProviderIndex(), providers.size() - 1);
    return providers.at(index);
}

QList<TextSelectionActionConfig> SettingModel::textSelectionActions() const
{
    const int count = settings_.value(KEY_TEXT_SELECTION_ACTIONS_COUNT, 0).toInt();
    if (count <= 0) {
        return defaultTextSelectionActions();
    }

    QList<TextSelectionActionConfig> actions;
    actions.reserve(count);
    for (int i = 0; i < count; ++i) {
        TextSelectionActionConfig action;
        action.id = settings_.value(textSelectionActionKey(i, "ID")).toString().trimmed();
        action.label = settings_.value(textSelectionActionKey(i, "LABEL")).toString().trimmed();
        action.prompt = settings_.value(textSelectionActionKey(i, "PROMPT")).toString().trimmed();
        if (action.id.isEmpty()) {
            action.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        actions.append(action);
    }

    return actions.isEmpty() ? defaultTextSelectionActions() : actions;
}

void SettingModel::setTextSelectionActions(const QList<TextSelectionActionConfig> &actions)
{
    const QList<TextSelectionActionConfig> effectiveActions =
        actions.isEmpty() ? defaultTextSelectionActions() : actions;

    const int oldCount = settings_.value(KEY_TEXT_SELECTION_ACTIONS_COUNT, 0).toInt();
    for (int i = effectiveActions.size(); i < oldCount; ++i) {
        settings_.remove(QString("LLM/TextSelectionActions/%1").arg(i));
    }

    settings_.setValue(KEY_TEXT_SELECTION_ACTIONS_COUNT, effectiveActions.size());
    for (int i = 0; i < effectiveActions.size(); ++i) {
        const TextSelectionActionConfig &action = effectiveActions.at(i);
        const QString id = action.id.trimmed().isEmpty()
            ? QUuid::createUuid().toString(QUuid::WithoutBraces)
            : action.id.trimmed();
        settings_.setValue(textSelectionActionKey(i, "ID"), id);
        settings_.setValue(textSelectionActionKey(i, "LABEL"), action.label.trimmed());
        settings_.setValue(textSelectionActionKey(i, "PROMPT"), action.prompt.trimmed());
    }
    settings_.sync();
}

bool SettingModel::llmImageTokenSavingEnabled() const
{
    return settings_.value(KEY_LLM_IMAGE_TOKEN_SAVING, true).toBool();
}

void SettingModel::setLlmImageTokenSavingEnabled(bool enabled)
{
    settings_.setValue(KEY_LLM_IMAGE_TOKEN_SAVING, enabled);
    settings_.sync();
}

GitHubImageBedConfig SettingModel::gitHubImageBedConfig() const
{
    GitHubImageBedConfig config;
    config.owner      = settings_.value(KEY_GITHUB_OWNER).toString();
    config.repo       = settings_.value(KEY_GITHUB_REPO).toString();
    config.branch     = settings_.value(KEY_GITHUB_BRANCH, "main").toString();
    config.token      = settings_.value(KEY_GITHUB_TOKEN).toString();
    config.pathPrefix = settings_.value(KEY_GITHUB_PATH_PREFIX, "img").toString();
    config.cdnUrl     = settings_.value(KEY_GITHUB_CDN_URL).toString();
    return config;
}

void SettingModel::setGitHubImageBedConfig(const GitHubImageBedConfig &config)
{
    settings_.setValue(KEY_GITHUB_OWNER,       config.owner);
    settings_.setValue(KEY_GITHUB_REPO,        config.repo);
    settings_.setValue(KEY_GITHUB_BRANCH,      config.branch);
    settings_.setValue(KEY_GITHUB_TOKEN,       config.token);
    settings_.setValue(KEY_GITHUB_PATH_PREFIX, config.pathPrefix);
    settings_.setValue(KEY_GITHUB_CDN_URL,     config.cdnUrl);
    settings_.sync();
}

PaddleOcrConfig SettingModel::paddleOcrConfig() const
{
    PaddleOcrConfig config;
    config.enabled = settings_.value(KEY_OCR_ENABLED, false).toBool();
    config.jobUrl = settings_.value(KEY_OCR_JOB_URL, DEFAULT_OCR_JOB_URL).toString();
    config.token = settings_.value(KEY_OCR_TOKEN, QString()).toString();
    config.model = settings_.value(KEY_OCR_MODEL, DEFAULT_OCR_MODEL).toString();
    return config;
}

void SettingModel::setPaddleOcrConfig(const PaddleOcrConfig &config)
{
    settings_.setValue(KEY_OCR_ENABLED, config.enabled);
    settings_.setValue(KEY_OCR_JOB_URL, config.jobUrl.trimmed());
    settings_.setValue(KEY_OCR_TOKEN, config.token.trimmed());
    settings_.setValue(KEY_OCR_MODEL, config.model.trimmed());
    settings_.sync();
}

HttpServerConfig SettingModel::httpServerConfig() const
{
    HttpServerConfig config;
    config.directory = settings_.value(KEY_HTTP_SERVER_DIR, QString()).toString();
    config.port = settings_.value(KEY_HTTP_SERVER_PORT, 8000).toInt();
    config.bind = settings_.value(KEY_HTTP_SERVER_BIND, QStringLiteral("0.0.0.0")).toString();
    config.protocol = settings_.value(KEY_HTTP_SERVER_PROTOCOL, QStringLiteral("HTTP/1.1")).toString();
    config.cgi = settings_.value(KEY_HTTP_SERVER_CGI, false).toBool();
    if (config.port < 1 || config.port > 65535) {
        config.port = 8000;
    }
    if (config.bind.trimmed().isEmpty()) {
        config.bind = QStringLiteral("0.0.0.0");
    }
    return config;
}

void SettingModel::setHttpServerConfig(const HttpServerConfig &config)
{
    settings_.setValue(KEY_HTTP_SERVER_DIR, config.directory.trimmed());
    settings_.setValue(KEY_HTTP_SERVER_PORT, config.port);
    settings_.setValue(KEY_HTTP_SERVER_BIND, config.bind.trimmed());
    settings_.setValue(KEY_HTTP_SERVER_PROTOCOL, config.protocol.trimmed());
    settings_.setValue(KEY_HTTP_SERVER_CGI, config.cgi);
    settings_.sync();
}

QSize SettingModel::notificationWindowSize() const
{
    const int width = settings_.value(KEY_NOTIFY_WIDTH, DEFAULT_NOTIFY_SIZE.width()).toInt();
    const int height = settings_.value(KEY_NOTIFY_HEIGHT, DEFAULT_NOTIFY_SIZE.height()).toInt();
    return QSize(qMax(DEFAULT_NOTIFY_SIZE.width(), width), qMax(DEFAULT_NOTIFY_SIZE.height(), height));
}

void SettingModel::setNotificationWindowSize(const QSize &size)
{
    settings_.setValue(KEY_NOTIFY_WIDTH, qMax(DEFAULT_NOTIFY_SIZE.width(), size.width()));
    settings_.setValue(KEY_NOTIFY_HEIGHT, qMax(DEFAULT_NOTIFY_SIZE.height(), size.height()));
    settings_.sync();
}

TrayNotificationPosition SettingModel::trayNotificationPosition() const
{
    const int value = settings_.value(
        KEY_NOTIFY_POS,
        static_cast<int>(TrayNotificationPosition::BottomRight)).toInt();
    if (value < static_cast<int>(TrayNotificationPosition::TopLeft) ||
        value > static_cast<int>(TrayNotificationPosition::BottomRight)) {
        return TrayNotificationPosition::BottomRight;
    }
    return static_cast<TrayNotificationPosition>(value);
}

void SettingModel::setTrayNotificationPosition(TrayNotificationPosition position)
{
    settings_.setValue(KEY_NOTIFY_POS, static_cast<int>(position));
    settings_.sync();
}
