#include "SettingModel.h"
#include <QApplication>
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
static const QString KEY_UI_LANGUAGE    = "UI_LANGUAGE";
static const QString KEY_PIN            = "PIN_GLOBAL_KEY";
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
static const QString KEY_AUTO_PIN       = "AUTO_PIN";
static const QString KEY_PIN_NO_BORDER  = "PIN_NO_BORDER";
static const QString KEY_RGB_COLOR      = "RGB_COLOR";
static const QString KEY_OPENCV_MODE    = "OPENCV_MODE";
static const QString KEY_AUTO_SAVE      = "AUTO_SAVE";
static const QString KEY_AUTO_SAVE_PATH = "AUTO_SAVE_PATH";
static const QString KEY_BG_ALPHA       = "BACKGROUND_COLOR_ALPHA";
static const QString KEY_BG_CHECKED     = "BACKGROUND_COLOR_CHECKED";
static const QString KEY_THEME_MODE     = "THEME_MODE";

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

static constexpr int LOCAL_SEARCH_HOTKEY_DEFAULT_VERSION = 2;
static constexpr int LOCAL_SEARCH_ROOTS_DEFAULT_VERSION = 1;
static constexpr int LOCAL_SEARCH_EXCLUDES_DEFAULT_VERSION = 2;

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
    setUiLanguage(QStringLiteral("en"));
    setScreenshotGlobalKey("F5");
    setPinGlobalKey("F6");

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

QString SettingModel::uiLanguage() const
{
    const QString language = settings_.value(KEY_UI_LANGUAGE, QStringLiteral("en")).toString();
    return language == QStringLiteral("zh") ? language : QStringLiteral("en");
}

void SettingModel::setUiLanguage(const QString& language)
{
    settings_.setValue(KEY_UI_LANGUAGE,
                       language == QStringLiteral("zh") ? QStringLiteral("zh") : QStringLiteral("en"));
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

// 对话窗口上次选的是“浏览器”（true）还是“联网”（false）。两者互斥，一个 bool 就够。

// 左栏（对话）宽度。返回 0 表示还没存过，调用方按当前布局决定。

// 右栏（浏览器）宽度。返回 0 表示还没存过，调用方回退到默认宽度。

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
        {QStringLiteral("bing"), QStringLiteral("Bing"), QCoreApplication::translate("App", "微软必应搜索"),
         QStringLiteral("https://www.bing.com/search"), QStringLiteral("q"),
         QStringLiteral(":/icons/bing.ico")},
        {QStringLiteral("google"), QStringLiteral("Google"), QCoreApplication::translate("App", "全球最大的搜索引擎"),
         QStringLiteral("https://www.google.com/search"), QStringLiteral("q"),
         QStringLiteral(":/icons/google.ico")},
        {QStringLiteral("baidu"), QCoreApplication::translate("App", "百度"), QCoreApplication::translate("App", "百度一下，你就知道"),
         QStringLiteral("https://www.baidu.com/s"), QStringLiteral("wd"),
         QStringLiteral(":/icons/baidu.ico")},
        {QStringLiteral("duckduckgo"), QStringLiteral("DuckDuckGo"), QCoreApplication::translate("App", "保护隐私的搜索引擎"),
         QStringLiteral("https://duckduckgo.com/"), QStringLiteral("q"),
         QStringLiteral(":/icons/duck.ico")},
        {QStringLiteral("perplexity"), QStringLiteral("Perplexity"), QCoreApplication::translate("App", "AI 驱动的答案引擎"),
         QStringLiteral("https://www.perplexity.ai/search"), QStringLiteral("q"),
         QStringLiteral(":/icons/perplexity.ico")},
        {QStringLiteral("metaso"), QCoreApplication::translate("App", "秘塔AI搜索"), QCoreApplication::translate("App", "没有广告，直达结果"),
         QStringLiteral("https://metaso.cn/"), QStringLiteral("q"),
         QStringLiteral(":/icons/metaso.ico")},
        {QStringLiteral("tiangong"), QCoreApplication::translate("App", "天工AI搜索"), QCoreApplication::translate("App", "新一代 AI 搜索"),
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
        QCoreApplication::translate("App", "# 版本控制与开发缓存"),
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
        QCoreApplication::translate("App", "# Windows 系统目录"),
        QStringLiteral("$Recycle.Bin/"),
        QStringLiteral("System Volume Information/"),
        QStringLiteral(""),
        QCoreApplication::translate("App", "# 临时文件与未完成下载"),
        QStringLiteral("*~"),
        QStringLiteral("*.tmp"),
        QStringLiteral("*.temp"),
        QStringLiteral("*.part"),
        QStringLiteral("*.crdownload")
    };
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
