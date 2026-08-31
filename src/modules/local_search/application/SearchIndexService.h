#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QHash>
#include <QSet>
#include <QStringList>

#include <memory>

class AsyncRunner;
class DirectoryChangeWatcher;
class SearchIndexStore;
class QTimer;

class SearchIndexService final : public QObject {
    Q_OBJECT
public:
    SearchIndexService(std::shared_ptr<SearchIndexStore> store, QObject* parent = nullptr);
    ~SearchIndexService() override;

    void applySettings(const QStringList& roots, const QStringList& excludePatterns,
                       const QStringList& bookmarkSources);
    void rebuildAll();
    QString statusText() const;
    QStringList roots() const { return roots_; }

signals:
    void statusChanged(const QString& status);
    void indexUpdated();

private:
    void enqueueRoot(const QString& root);
    void startNext();
    void setStatus(const QString& status);
    void scheduleRefresh(const QString& root);
    void updateProgress(qint64 processed);
    int progressPercent() const;
    void finishRebuild();

    std::shared_ptr<SearchIndexStore> store_;
    AsyncRunner* runner_ = nullptr;
    DirectoryChangeWatcher* watcher_ = nullptr;
    QTimer* refreshTimer_ = nullptr;
    QTimer* progressTimer_ = nullptr;
    QStringList roots_;
    QStringList excludePatterns_;
    QStringList bookmarkSources_;
    QStringList queue_;
    QSet<QString> queued_;
    QSet<QString> pendingRefresh_;
    QString statusText_;
    QString activeRoot_;
    QHash<QString, qint64> rootWeights_;
    QElapsedTimer rebuildTimer_;
    qint64 totalWeight_ = 0;
    qint64 completedWeight_ = 0;
    qint64 activeProcessed_ = 0;
    bool rebuilding_ = false;
    bool fullRebuildPending_ = false;
};
