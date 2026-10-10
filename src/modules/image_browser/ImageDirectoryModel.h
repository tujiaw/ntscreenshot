#pragma once

#include <QAbstractListModel>
#include <QCache>
#include <QFileInfo>
#include <QImage>
#include <QSet>
#include <atomic>
#include <memory>
#include <functional>

namespace ImageBrowser {
class ImageLoader;

class ImageDirectoryModel final : public QAbstractListModel {
    Q_OBJECT
public:
    explicit ImageDirectoryModel(ImageLoader* loader, QObject* parent = nullptr);
    ~ImageDirectoryModel() override;
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QString pathAt(int row) const;
    int indexOf(const QString& path) const;
    void scan(const QString& directory);
    void requestRange(int first, int last, QSize size);
    static QList<QFileInfo> enumerate(const QString& directory, QString* error = nullptr,
                                     const std::function<bool()>& cancelled = {});
    int cacheBytes() const { return cache_.totalCost(); }

signals:
    void scanned(const QString& error);

private:
    QString cacheKey(int row, QSize size) const;
    void pump();
    ImageLoader* loader_;
    QList<QFileInfo> files_;
    mutable QCache<QString, QImage> cache_{64 * 1024 * 1024};
    QSet<QString> failed_;
    QSet<QString> pending_;
    QList<int> wanted_;
    QSize thumbnailSize_{160, 100};
    int active_ = 0;
    quint64 scanVersion_ = 0;
    std::shared_ptr<std::atomic<quint64>> scanGeneration_ = std::make_shared<std::atomic<quint64>>(0);
    std::shared_ptr<std::atomic_bool> alive_ = std::make_shared<std::atomic_bool>(true);
};
} // namespace ImageBrowser
