#include "modules/local_search/application/SearchIndexService.h"

#include "core/foundation/AsyncRunner.h"
#include "shared/foundation/IgnorePatternMatcher.h"
#include "modules/local_search/domain/SearchTypes.h"
#include "modules/local_search/infrastructure/DirectoryChangeWatcher.h"
#include "modules/local_search/infrastructure/BrowserBookmarkReader.h"
#include "modules/local_search/infrastructure/SearchIndexStore.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTimer>
#include <QThread>

#include <algorithm>
#include <functional>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#endif

namespace {

const QString kStartMenuApplicationsRoot = QStringLiteral("::applications:start-menu::");
const QString kAppsFolderApplicationsRoot = QStringLiteral("::applications:apps-folder::");
const QString kBookmarksRoot = QStringLiteral("::bookmarks::");

bool isApplicationRoot(const QString& root)
{
    return root == kStartMenuApplicationsRoot || root == kAppsFolderApplicationsRoot;
}

QString formatElapsed(qint64 elapsedMs)
{
    const qint64 totalSeconds = qMax<qint64>(0, elapsedMs / 1000);
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;
    return hours > 0
        ? QStringLiteral("%1:%2:%3")
              .arg(hours, 2, 10, QLatin1Char('0'))
              .arg(minutes, 2, 10, QLatin1Char('0'))
              .arg(seconds, 2, 10, QLatin1Char('0'))
        : QStringLiteral("%1:%2")
              .arg(minutes, 2, 10, QLatin1Char('0'))
              .arg(seconds, 2, 10, QLatin1Char('0'));
}

QString normalizedPath(const QString& path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

bool isReparsePoint(const QFileInfo& info)
{
#ifdef Q_OS_WIN
    const std::wstring nativePath = QDir::toNativeSeparators(info.absoluteFilePath()).toStdWString();
    const DWORD attributes = GetFileAttributesW(nativePath.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES
           && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
    return info.isSymLink();
#endif
}

SearchIndexItem toIndexItem(const QFileInfo& info, SearchItemType type,
                            const QString& root)
{
    SearchIndexItem item;
    item.type = type;
    item.name = type == SearchItemType::Application ? info.completeBaseName() : info.fileName();
    item.path = normalizedPath(info.absoluteFilePath());
    item.parentPath = info.absolutePath();
    item.extension = info.suffix().toLower();
    item.modifiedAt = info.lastModified().toMSecsSinceEpoch();
    item.launchTarget = item.path;
    item.sourceRoot = root;
    return item;
}

QVector<SearchIndexItem> scanDirectory(const QString& root, const QStringList& exclusions,
                                       const std::function<void(qint64)>& progress)
{
    QVector<SearchIndexItem> items;
    const IgnorePatternMatcher matcher(exclusions);
    QStringList pending{root};
    while (!pending.isEmpty() && !QThread::currentThread()->isInterruptionRequested()) {
        const QString directoryPath = pending.takeLast();
        QDir directory(directoryPath);
        const QFileInfoList entries = directory.entryInfoList(
            QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden | QDir::System,
            QDir::DirsFirst | QDir::Name);
        for (const QFileInfo& info : entries) {
            const QString path = info.absoluteFilePath();
            const QString relativePath = QDir(root).relativeFilePath(path);
            const bool isExcluded = matcher.isExcluded(relativePath, info.isDir());
            if (info.isSymLink() || isReparsePoint(info)) continue;
            if (isExcluded) continue;
            const SearchItemType type = info.isDir() ? SearchItemType::Directory : SearchItemType::File;
            items.push_back(toIndexItem(info, type, root));
            if (progress && (items.size() % 256 == 0)) progress(items.size());
            if (info.isDir()) pending.push_back(path);
        }
    }
    if (progress) progress(items.size());
    return items;
}

QStringList applicationDirectories()
{
    QStringList directories = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
#ifdef Q_OS_WIN
    const QString appData = qEnvironmentVariable("APPDATA");
    const QString programData = qEnvironmentVariable("ProgramData");
    if (!appData.isEmpty()) directories << appData + QStringLiteral("/Microsoft/Windows/Start Menu/Programs");
    if (!programData.isEmpty()) directories << programData + QStringLiteral("/Microsoft/Windows/Start Menu/Programs");
#endif
    directories.removeDuplicates();
    return directories;
}

QVector<SearchIndexItem> scanStartMenuApplications()
{
    QVector<SearchIndexItem> items;
    for (const QString& root : applicationDirectories()) {
        QStringList pending{root};
        while (!pending.isEmpty()) {
            QDir directory(pending.takeLast());
            const QFileInfoList entries = directory.entryInfoList(
                QDir::NoDotAndDotDot | QDir::AllEntries, QDir::DirsFirst | QDir::Name);
            for (const QFileInfo& info : entries) {
                if (info.isDir()) {
                    pending.push_back(info.absoluteFilePath());
                } else if (info.suffix().compare(QStringLiteral("lnk"), Qt::CaseInsensitive) == 0 ||
                           info.suffix().compare(QStringLiteral("exe"), Qt::CaseInsensitive) == 0) {
                    items.push_back(toIndexItem(info, SearchItemType::Application,
                                                kStartMenuApplicationsRoot));
                }
            }
        }
    }
    return items;
}

struct ApplicationScanResult {
    QVector<SearchIndexItem> items;
    QString error;
};

ApplicationScanResult scanAppsFolderApplications()
{
    ApplicationScanResult result;
#ifdef Q_OS_WIN
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool uninitializeCom = SUCCEEDED(comResult);
    if (FAILED(comResult) && comResult != RPC_E_CHANGED_MODE) {
        result.error = QStringLiteral("COM 初始化失败：0x%1")
                           .arg(static_cast<quint32>(comResult), 8, 16, QLatin1Char('0'));
        return result;
    }
    PIDLIST_ABSOLUTE appsPidl = nullptr;
    IShellFolder* appsFolder = nullptr;
    const HRESULT folderResult = SHGetKnownFolderIDList(
        FOLDERID_AppsFolder, KF_FLAG_DEFAULT, nullptr, &appsPidl);
    const HRESULT bindResult = SUCCEEDED(folderResult)
        ? SHBindToObject(nullptr, appsPidl, nullptr, IID_PPV_ARGS(&appsFolder))
        : folderResult;
    if (SUCCEEDED(folderResult) && SUCCEEDED(bindResult)) {
        IEnumIDList* enumerator = nullptr;
        const HRESULT enumResult = appsFolder->EnumObjects(
            nullptr, SHCONTF_NONFOLDERS, &enumerator);
        if (SUCCEEDED(enumResult)) {
            PITEMID_CHILD child = nullptr;
            while (enumerator->Next(1, &child, nullptr) == S_OK) {
                IShellItem* shellItem = nullptr;
                if (SUCCEEDED(SHCreateItemWithParent(appsPidl, appsFolder, child,
                                                     IID_PPV_ARGS(&shellItem)))) {
                    PWSTR displayName = nullptr;
                    PWSTR parsingName = nullptr;
                    if (SUCCEEDED(shellItem->GetDisplayName(SIGDN_NORMALDISPLAY, &displayName)) &&
                        SUCCEEDED(shellItem->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING,
                                                            &parsingName))) {
                        SearchIndexItem item;
                        item.type = SearchItemType::Application;
                        item.name = QString::fromWCharArray(displayName);
                        item.path = QString::fromWCharArray(parsingName);
                        item.launchTarget = QFileInfo::exists(item.path)
                            || item.path.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
                            || item.path.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)
                            ? item.path
                            : QStringLiteral("shell:AppsFolder\\") + item.path;
                        item.sourceRoot = kAppsFolderApplicationsRoot;
                        result.items.push_back(std::move(item));
                    }
                    CoTaskMemFree(displayName);
                    CoTaskMemFree(parsingName);
                    shellItem->Release();
                }
                CoTaskMemFree(child);
            }
            enumerator->Release();
        } else {
            result.error = QStringLiteral("AppsFolder 枚举失败：0x%1")
                               .arg(static_cast<quint32>(enumResult), 8, 16, QLatin1Char('0'));
        }
        appsFolder->Release();
    } else {
        const HRESULT errorResult = FAILED(folderResult) ? folderResult : bindResult;
        result.error = QStringLiteral("AppsFolder 打开失败：0x%1")
                           .arg(static_cast<quint32>(errorResult), 8, 16, QLatin1Char('0'));
    }
    CoTaskMemFree(appsPidl);
    if (uninitializeCom) CoUninitialize();
#else
#endif
    return result;
}

} // namespace

SearchIndexService::SearchIndexService(std::shared_ptr<SearchIndexStore> store, QObject* parent)
    : QObject(parent)
    , store_(std::move(store))
    , runner_(new AsyncRunner(this))
    , watcher_(new DirectoryChangeWatcher(this))
    , refreshTimer_(new QTimer(this))
    , progressTimer_(new QTimer(this))
{
    qInfo() << "SearchIndexService: created";
    refreshTimer_->setSingleShot(true);
    refreshTimer_->setInterval(1200);
    connect(refreshTimer_, &QTimer::timeout, this, [this]() {
        const auto roots = pendingRefresh_.values();
        pendingRefresh_.clear();
        for (const QString& root : roots) enqueueRoot(root);
    });
    connect(watcher_, &DirectoryChangeWatcher::rootChanged,
            this, &SearchIndexService::scheduleRefresh);
    connect(watcher_, &DirectoryChangeWatcher::rootUnavailable, this, [this](const QString& root) {
        setStatus(QStringLiteral("监听异常，即将校验：%1").arg(root));
        scheduleRefresh(root);
    });
    progressTimer_->setInterval(1000);
    connect(progressTimer_, &QTimer::timeout, this, [this]() {
        if (rebuilding_) emit statusChanged(statusText());
    });
}

SearchIndexService::~SearchIndexService()
{
    qInfo() << "SearchIndexService: destructing, stopping watcher and runner...";
    watcher_->stop();
    runner_->cancel();
    qInfo() << "SearchIndexService: destructed";
}

void SearchIndexService::applySettings(const QStringList& roots, const QStringList& excludePatterns,
                                       const QStringList& bookmarkSources)
{
    QStringList candidates;
    for (const QString& root : roots) {
        const QString normalized = normalizedPath(root);
        if (QFileInfo(normalized).isDir() && !candidates.contains(normalized, Qt::CaseInsensitive)) {
            candidates.push_back(normalized);
        }
    }
    std::sort(candidates.begin(), candidates.end(), [](const QString& left, const QString& right) {
        return left.size() < right.size();
    });
    QStringList normalizedRoots;
    for (const QString& candidate : candidates) {
        bool covered = false;
        for (const QString& parent : normalizedRoots) {
            const QString prefix = parent.endsWith(u'/') || parent.endsWith(u'\\')
                                       ? parent : parent + QDir::separator();
            if (candidate.startsWith(prefix, Qt::CaseInsensitive)) {
                covered = true;
                break;
            }
        }
        if (!covered) normalizedRoots.push_back(candidate);
    }
    const QStringList previousRoots = roots_;
    const bool rootsChanged = normalizedRoots != roots_;
    QStringList normalizedBookmarkSources;
    for (const QString& source : bookmarkSources) {
        const QString value = source.trimmed().toLower();
        if ((value == QStringLiteral("chrome") || value == QStringLiteral("edge"))
            && !normalizedBookmarkSources.contains(value)) {
            normalizedBookmarkSources.push_back(value);
        }
    }
    const bool bookmarkSourcesChanged = normalizedBookmarkSources != bookmarkSources_;
    const bool changed = rootsChanged || excludePatterns != excludePatterns_
                         || bookmarkSourcesChanged;
    roots_ = normalizedRoots;
    excludePatterns_ = excludePatterns;
    bookmarkSources_ = normalizedBookmarkSources;
    for (const QString& previous : previousRoots) {
        if (!roots_.contains(previous, Qt::CaseInsensitive)) store_->removeRoot(previous);
    }
    if (rootsChanged) watcher_->start(roots_);
    if (bookmarkSources_.isEmpty()) store_->removeRoot(kBookmarksRoot);
    if (changed || store_->itemCount() == 0) {
        qInfo() << "SearchIndexService::applySettings: changed, triggering rebuildAll...";
        rebuildAll();
    }
    qInfo() << "SearchIndexService::applySettings: done, normalized roots =" << roots_.size()
            << "item count =" << store_->itemCount();
}

void SearchIndexService::rebuildAll()
{
    qInfo() << "SearchIndexService::rebuildAll: start, runner busy =" << runner_->busy();
    if (runner_->busy()) {
        qInfo() << "SearchIndexService::rebuildAll: runner busy, deferring...";
        fullRebuildPending_ = true;
        return;
    }
    queue_.clear();
    queued_.clear();
    rootWeights_.clear();
    totalWeight_ = 0;
    completedWeight_ = 0;
    activeProcessed_ = 0;
    activeRoot_.clear();
    rebuilding_ = true;
    rebuildTimer_.restart();
    progressTimer_->start();

    QStringList plannedRoots{kStartMenuApplicationsRoot, kAppsFolderApplicationsRoot};
    if (!bookmarkSources_.isEmpty()) plannedRoots.push_back(kBookmarksRoot);
    plannedRoots.append(roots_);
    for (const QString& root : plannedRoots) {
        const qint64 weight = qMax<qint64>(100, store_->itemCountForRoot(root));
        rootWeights_.insert(root, weight);
        totalWeight_ += weight;
    }
    // Applications are immediately useful and cheap to enumerate, so make them
    // available before potentially long user-directory scans.
    enqueueRoot(kStartMenuApplicationsRoot);
    enqueueRoot(kAppsFolderApplicationsRoot);
    if (!bookmarkSources_.isEmpty()) enqueueRoot(kBookmarksRoot);
    for (const QString& root : roots_) enqueueRoot(root);
    qInfo() << "SearchIndexService::rebuildAll: enqueued" << queue_.size() << "roots, totalWeight =" << totalWeight_;
}

void SearchIndexService::enqueueRoot(const QString& root)
{
    if (root.isEmpty() || queued_.contains(root)) return;
    qInfo() << "SearchIndexService::enqueueRoot:" << root;
    queued_.insert(root);
    queue_.push_back(root);
    startNext();
}

void SearchIndexService::startNext()
{
    if (runner_->busy()) return;
    if (queue_.isEmpty()) {
        qInfo() << "SearchIndexService::startNext: queue empty, rebuilding =" << rebuilding_
                << "fullRebuildPending =" << fullRebuildPending_;
        if (rebuilding_) {
            finishRebuild();
        } else if (fullRebuildPending_) {
            fullRebuildPending_ = false;
            rebuildAll();
        }
        return;
    }
    const QString root = queue_.takeFirst();
    qInfo() << "SearchIndexService::startNext: processing root =" << root
            << "remaining in queue =" << queue_.size();
    queued_.remove(root);
    activeRoot_ = root;
    activeProcessed_ = 0;
    setStatus(isApplicationRoot(root)
                  ? QStringLiteral("正在索引应用…")
                  : root == kBookmarksRoot
                      ? QStringLiteral("正在导入浏览器书签…")
                      : QStringLiteral("正在索引：%1").arg(root));
    auto items = std::make_shared<QVector<SearchIndexItem>>();
    auto error = std::make_shared<QString>();
    const QStringList exclusions = excludePatterns_;
    const QStringList bookmarkSources = bookmarkSources_;
    runner_->start([this, root, exclusions, bookmarkSources, items, error]() {
        qInfo() << "SearchIndexService: background scan started for root =" << root;
        if (root == kStartMenuApplicationsRoot) {
            *items = scanStartMenuApplications();
        } else if (root == kAppsFolderApplicationsRoot) {
            ApplicationScanResult result = scanAppsFolderApplications();
            *items = std::move(result.items);
            *error = std::move(result.error);
        } else if (root == kBookmarksRoot) {
            *items = BrowserBookmarkReader::readSelected(bookmarkSources);
        } else {
            *items = scanDirectory(root, exclusions, [this](qint64 processed) {
                QMetaObject::invokeMethod(this, [this, processed]() {
                    updateProgress(processed);
                }, Qt::QueuedConnection);
            });
        }
        // Keep the last valid application index when Windows enumeration fails.
        if (!error->isEmpty()) return;
        store_->replaceRoot(root, *items, error.get());
    }, [this, root, items, error]() {
        qInfo() << "SearchIndexService: background scan finished for root =" << root
                << "scanned items =" << items->size()
                << "error =" << (error->isEmpty() ? QStringLiteral("none") : *error);
        if (!error->isEmpty()) {
            setStatus(QStringLiteral("索引失败：%1").arg(*error));
        } else {
            setStatus(QStringLiteral("索引就绪：%1 项").arg(store_->itemCount()));
            emit indexUpdated();
        }
        completedWeight_ += rootWeights_.value(root, 100);
        activeProcessed_ = 0;
        activeRoot_.clear();
        startNext();
    });
}

void SearchIndexService::setStatus(const QString& status)
{
    statusText_ = status;
    emit statusChanged(statusText());
}

void SearchIndexService::scheduleRefresh(const QString& root)
{
    pendingRefresh_.insert(root);
    refreshTimer_->start();
}

QString SearchIndexService::statusText() const
{
    if (!rebuilding_) return statusText_;
    return QStringLiteral("重建索引 %1% · 已用时 %2 · %3")
        .arg(progressPercent())
        .arg(formatElapsed(rebuildTimer_.isValid() ? rebuildTimer_.elapsed() : 0), statusText_);
}

void SearchIndexService::updateProgress(qint64 processed)
{
    if (!rebuilding_ || activeRoot_.isEmpty()) return;
    activeProcessed_ = qMax(activeProcessed_, processed);
    emit statusChanged(statusText());
}

int SearchIndexService::progressPercent() const
{
    if (!rebuilding_ || totalWeight_ <= 0) return rebuilding_ ? 0 : 100;
    const qint64 activeWeight = rootWeights_.value(activeRoot_, 0);
    const qint64 activeContribution = activeWeight > 0
        ? qMin(qMax<qint64>(0, activeWeight - 1), activeProcessed_)
        : 0;
    return qBound(0, static_cast<int>((completedWeight_ + activeContribution) * 100
                                      / totalWeight_), 99);
}

void SearchIndexService::finishRebuild()
{
    if (!rebuilding_) return;
    const QString elapsed = formatElapsed(rebuildTimer_.isValid() ? rebuildTimer_.elapsed() : 0);
    qInfo() << "SearchIndexService::finishRebuild: total items =" << store_->itemCount()
            << "elapsed =" << elapsed;
    rebuilding_ = false;
    progressTimer_->stop();
    statusText_ = QStringLiteral("索引就绪：%1 项 · 耗时 %2").arg(store_->itemCount()).arg(elapsed);
    emit statusChanged(statusText_);
    if (fullRebuildPending_) {
        qInfo() << "SearchIndexService::finishRebuild: fullRebuildPending, triggering rebuild...";
        fullRebuildPending_ = false;
        rebuildAll();
    }
}
