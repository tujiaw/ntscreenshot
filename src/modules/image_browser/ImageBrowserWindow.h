#pragma once

#include "ImageDocument.h"
#include <QMainWindow>
#include <QFileSystemWatcher>
#include <QTimer>
#include <atomic>
#include <memory>
#include <functional>

class QAction;
class QLabel;
class QListView;
class QMovie;
class QSplitter;
class QToolBar;
class SettingModel;

namespace ImageBrowser {
class ImageLoader;
class ImageDirectoryModel;
class ImageCanvas;

class ImageBrowserWindow final : public QMainWindow {
    Q_OBJECT
public:
    ImageBrowserWindow(SettingModel* settings, ImageLoader* loader);
    ~ImageBrowserWindow() override;

protected:
    void closeEvent(QCloseEvent*) override;
    void dragEnterEvent(QDragEnterEvent*) override;
    void dropEvent(QDropEvent*) override;
    void hideEvent(QHideEvent*) override;
    void showEvent(QShowEvent*) override;
    void changeEvent(QEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;
    bool eventFilter(QObject*, QEvent*) override;

private:
    void openDirectory(const QString& directory, const QString& selection = {});
    void refresh();
    void listingReady(const QString& error);
    void selectRow(int row);
    void loadImage(const QString& path);
    void updateActions();
    void requestThumbnails();
    bool discardOrSave();
    bool save(bool overwrite = false, bool refreshListing = true, const QByteArray& format = {});
    void convertFormat();
    void showContextMenu(const QPoint& globalPosition);
    void edit(const EditOperation& operation);
    void renderDocument(ImageDocument candidate);
    void resizeImage();
    void freezeFrame(std::function<void()> after = {});
    void stopMovie();
    void screenshotDirectory();
    void updateStatus();

private slots:
    void applyDpi(double scale = 0);

private:

    SettingModel* settings_;
    ImageLoader* loader_;
    ImageDirectoryModel* model_;
    ImageCanvas* canvas_;
    QListView* list_;
    QSplitter* splitter_;
    QToolBar* toolbar_;
    QLabel* pathLabel_;
    QLabel* details_ = nullptr;
    QMovie* movie_ = nullptr;
    QFileSystemWatcher directoryWatcher_;
    QTimer refreshTimer_;
    QTimer thumbnailsTimer_;
    QString directory_;
    QString pendingSelection_;
    ImageDocument document_;
    QSize displayedSize_;
    quint64 imageVersion_ = 0;
    std::shared_ptr<std::atomic<quint64>> loadGeneration_ = std::make_shared<std::atomic<quint64>>(0);
    bool busy_ = false;
    bool loading_ = false;
    bool scanning_ = false;
    bool animationSource_ = false;
    bool resumeMovie_ = false;
    QAction* saveAction_;
    QAction* overwriteAction_;
    QAction* copyAction_;
    QAction* cropAction_;
    QAction* applyCropAction_;
    QAction* cancelCropAction_;
    QAction* undoAction_;
    QAction* redoAction_;
    QAction* resetAction_;
    QAction* playAction_;
    QAction* frameAction_;
    QAction* convertAction_;
    QList<QAction*> editActions_;
    QList<QAction*> navigationActions_;
};
} // namespace ImageBrowser
