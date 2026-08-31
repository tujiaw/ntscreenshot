#include "modules/local_search/infrastructure/BrowserBookmarkReader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QUrl>

namespace {

const QString kBookmarksRoot = QStringLiteral("::bookmarks::");

void appendNodes(const QJsonArray& nodes, const QString& browserName,
                 const QString& folder, QVector<SearchIndexItem>* items)
{
    for (const QJsonValue& value : nodes) {
        const QJsonObject node = value.toObject();
        const QString type = node.value(QStringLiteral("type")).toString();
        const QString name = node.value(QStringLiteral("name")).toString().trimmed();
        if (type == QStringLiteral("folder")) {
            const QString childFolder = folder.isEmpty() ? name : folder + QStringLiteral(" / ") + name;
            appendNodes(node.value(QStringLiteral("children")).toArray(), browserName,
                        childFolder, items);
            continue;
        }
        if (type != QStringLiteral("url")) continue;
        const QUrl url(node.value(QStringLiteral("url")).toString());
        if (!url.isValid() || (url.scheme() != QStringLiteral("http")
                               && url.scheme() != QStringLiteral("https"))) {
            continue;
        }
        SearchIndexItem item;
        item.type = SearchItemType::Bookmark;
        item.name = name.isEmpty() ? url.host() : name;
        item.path = url.toString(QUrl::FullyEncoded);
        item.parentPath = folder.isEmpty() ? browserName
                                           : browserName + QStringLiteral(" · ") + folder;
        item.launchTarget = item.path;
        item.sourceRoot = kBookmarksRoot;
        items->push_back(std::move(item));
    }
}

QString browserUserDataPath(const QString& source)
{
#ifdef Q_OS_WIN
    const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    if (source == QStringLiteral("chrome"))
        return localAppData + QStringLiteral("/Google/Chrome/User Data");
    if (source == QStringLiteral("edge"))
        return localAppData + QStringLiteral("/Microsoft/Edge/User Data");
#elif defined(Q_OS_MACOS)
    const QString home = QDir::homePath();
    if (source == QStringLiteral("chrome"))
        return home + QStringLiteral("/Library/Application Support/Google/Chrome");
    if (source == QStringLiteral("edge"))
        return home + QStringLiteral("/Library/Application Support/Microsoft Edge");
#else
    const QString home = QDir::homePath();
    if (source == QStringLiteral("chrome")) return home + QStringLiteral("/.config/google-chrome");
    if (source == QStringLiteral("edge")) return home + QStringLiteral("/.config/microsoft-edge");
#endif
    return {};
}

QString displayName(const QString& source)
{
    return source == QStringLiteral("edge") ? QStringLiteral("Edge") : QStringLiteral("Chrome");
}

} // namespace

QVector<SearchIndexItem> BrowserBookmarkReader::readSelected(const QStringList& sources)
{
    QVector<SearchIndexItem> items;
    QSet<QString> seenUrls;
    for (const QString& sourceValue : sources) {
        const QString source = sourceValue.trimmed().toLower();
        const QDir userData(browserUserDataPath(source));
        if (!userData.exists()) continue;
        const QFileInfoList profiles = userData.entryInfoList(
            QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo& profile : profiles) {
            const QString path = profile.absoluteFilePath() + QStringLiteral("/Bookmarks");
            if (!QFileInfo::exists(path)) continue;
            const QVector<SearchIndexItem> profileItems = readFile(path, displayName(source));
            for (const SearchIndexItem& item : profileItems) {
                const QString key = item.path.toCaseFolded();
                if (!seenUrls.contains(key)) {
                    seenUrls.insert(key);
                    items.push_back(item);
                }
            }
        }
    }
    return items;
}

QVector<SearchIndexItem> BrowserBookmarkReader::readFile(const QString& filePath,
                                                         const QString& browserName)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return {};
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) return {};

    QVector<SearchIndexItem> items;
    const QJsonObject roots = document.object().value(QStringLiteral("roots")).toObject();
    for (auto it = roots.constBegin(); it != roots.constEnd(); ++it) {
        const QJsonObject root = it.value().toObject();
        appendNodes(root.value(QStringLiteral("children")).toArray(), browserName,
                    root.value(QStringLiteral("name")).toString(), &items);
    }
    return items;
}
