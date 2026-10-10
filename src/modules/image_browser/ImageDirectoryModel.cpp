#include "ImageDirectoryModel.h"
#include "ImageDocument.h"
#include "ImageLoader.h"

#include <QDir>
#include <QDirIterator>
#include <QImageReader>
#include <QPixmap>
#include <algorithm>

namespace ImageBrowser {
ImageDirectoryModel::ImageDirectoryModel(ImageLoader* loader, QObject* parent)
    : QAbstractListModel(parent), loader_(loader) {}
ImageDirectoryModel::~ImageDirectoryModel() { alive_->store(false); scanGeneration_->store(++scanVersion_); }

int ImageDirectoryModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : files_.size(); }
QString ImageDirectoryModel::pathAt(int row) const {
    return row >= 0 && row < files_.size() ? files_[row].absoluteFilePath() : QString();
}
int ImageDirectoryModel::indexOf(const QString& path) const {
    for (int i = 0; i < files_.size(); ++i) if (files_[i].absoluteFilePath() == path) return i;
    return -1;
}
QString ImageDirectoryModel::cacheKey(int row, QSize size) const {
    const auto& f = files_[row];
    return f.absoluteFilePath() + QStringLiteral("|%1|%2|%3x%4")
        .arg(f.lastModified().toMSecsSinceEpoch()).arg(f.size()).arg(size.width()).arg(size.height());
}
QVariant ImageDirectoryModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= files_.size()) return {};
    if (role == Qt::DisplayRole) return files_[index.row()].fileName();
    if (role == Qt::ToolTipRole) return pathAt(index.row());
    if (role == Qt::DecorationRole) {
        if (const QImage* image = cache_.object(cacheKey(index.row(), thumbnailSize_))) return QPixmap::fromImage(*image);
    }
    if (role == Qt::UserRole) return failed_.contains(cacheKey(index.row(), thumbnailSize_));
    return {};
}

QList<QFileInfo> ImageDirectoryModel::enumerate(const QString& directory, QString* error,
                                              const std::function<bool()>& cancelled) {
    QList<QFileInfo> files;
    const QFileInfo root(directory);
    if (!root.isDir() || !root.isReadable()) {
        if (error) *error = tr("目录不存在或无法读取。");
        return files;
    }
    QSet<QString> extensions;
    for (const auto& format : QImageReader::supportedImageFormats()) extensions.insert(QString::fromLatin1(format).toLower());
    if (extensions.contains(QStringLiteral("jpeg")) || extensions.contains(QStringLiteral("jpg"))) {
        extensions.insert(QStringLiteral("jpg"));
        extensions.insert(QStringLiteral("jpeg"));
    }
    if (extensions.contains(QStringLiteral("tiff")) || extensions.contains(QStringLiteral("tif"))) {
        extensions.insert(QStringLiteral("tif"));
        extensions.insert(QStringLiteral("tiff"));
    }
    QDirIterator it(directory, QDir::Files | QDir::Readable | QDir::NoDotAndDotDot);
    while (it.hasNext()) {
        if (cancelled && cancelled()) return {};
        it.next();
        const auto info = it.fileInfo();
        // Known extensions include broken images, so they remain navigable.
        if (extensions.contains(info.suffix().toLower()) || !QImageReader::imageFormat(info.absoluteFilePath()).isEmpty())
            files.append(info);
    }
    std::sort(files.begin(), files.end(), [](const QFileInfo& a, const QFileInfo& b) {
        if (a.lastModified() != b.lastModified()) return a.lastModified() > b.lastModified();
        return a.fileName() < b.fileName();
    });
    return files;
}

void ImageDirectoryModel::scan(const QString& directory) {
    const quint64 version = ++scanVersion_;
    scanGeneration_->store(version);
    wanted_.clear();
    const auto alive = alive_;
    const auto generation = scanGeneration_;
    struct Listing { QList<QFileInfo> files; QString error; };
    loader_->run(this, [directory, alive, generation, version] {
        Listing listing;
        if (alive->load()) listing.files = enumerate(directory, &listing.error, [generation, version] {
            return generation->load() != version;
        });
        return listing;
    }, [this, version](const Listing& listing) {
        if (version != scanVersion_) return;
        beginResetModel();
        files_ = listing.files;
        failed_.clear();
        endResetModel();
        emit scanned(listing.error);
    });
}

void ImageDirectoryModel::requestRange(int first, int last, QSize size) {
    thumbnailSize_ = size;
    wanted_.clear();
    for (int row = qMax(0, first); row <= qMin(last, int(files_.size()) - 1); ++row) wanted_.append(row);
    pump();
}

void ImageDirectoryModel::pump() {
    while (active_ < 2 && !wanted_.isEmpty()) {
        const int row = wanted_.takeFirst();
        if (row >= files_.size()) continue;
        const QString key = cacheKey(row, thumbnailSize_);
        if (cache_.contains(key) || failed_.contains(key) || pending_.contains(key)) continue;
        pending_.insert(key);
        ++active_;
        const QString path = pathAt(row);
        const QSize size = thumbnailSize_;
        const quint64 version = scanVersion_;
        const auto alive = alive_;
        loader_->run(this, [path, size, alive] {
            if (!alive->load()) return ImageResult{};
            auto result = ImageDocument::read(path, size);
            if (!result.image.isNull()) result.image = result.image.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            return result;
        }, [this, key, version, row](const ImageResult& result) {
            --active_;
            pending_.remove(key);
            if (version == scanVersion_ && row < files_.size()) {
                if (result.image.isNull()) failed_.insert(key);
                else cache_.insert(key, new QImage(result.image), int(result.image.sizeInBytes()));
                emit dataChanged(index(row), index(row), {Qt::DecorationRole, Qt::UserRole});
            }
            pump();
        });
    }
}
} // namespace ImageBrowser
