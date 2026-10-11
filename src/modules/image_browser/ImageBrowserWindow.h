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
class QToolButton;
class SettingModel;

namespace ImageBrowser {
class ImageLoader;
class ImageDirectoryModel;
class ImageCanvas;
class ImageEditorPanel;

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
    bool save(bool overwrite = false, bool refreshListing = true, const QByteArray& format = {},
              const ImageConversionOptions& options = {});
    void convertFormat();
    void setSystemBackground(bool lockScreen);
    void showContextMenu(const QPoint& globalPosition);
    void edit(const EditOperation& operation);
    void renderDocument(ImageDocument candidate);
    void resizeImage();
    void freezeFrame(std::function<void()> after = {});
    void stopMovie();
    void screenshotDirectory();
    void updateStatus();
    void toggleFullscreen();
    void showStatus(const QString& message, int timeout = 0);
    void styleToolButtons();
    void positionTitle();
    void positionOverlays();
    void updateOverlays();
    void syncControlButtons();
    void layoutControlBar();
    QToolButton* controlButton(QAction* action);
    void setZoomStep(double factor);
    void createEditorPanel();
    void positionEditorPanel();
    void selectEditRegion(const EditOperation& operation);
    void applyEditRegion(const QRect& rect);

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
    ImageEditorPanel* editorPanel_ = nullptr;
    QAction* editorAction_ = nullptr;
    EditOperation regionOperation_;
    bool regionFromPanel_ = false;
    QLabel* details_ = nullptr;
    QLabel* titleLabel_ = nullptr;
    QLabel* counterLabel_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QWidget* controlBar_ = nullptr;
    QWidget* controlLeft_ = nullptr;
    QWidget* controlRight_ = nullptr;
    QWidget* controlCopySave_ = nullptr;
    QWidget* toolPill_ = nullptr;
    QToolButton* zoomButton_ = nullptr;
    QToolButton* overlayPrevious_ = nullptr;
    QToolButton* overlayNext_ = nullptr;
    QToolButton* sidebarButton_ = nullptr;
    QList<QToolButton*> controlButtons_;
    QString titleText_;
    QString statusText_;
    QAction* spacerAction_ = nullptr;
    QMovie* movie_ = nullptr;
    QFileSystemWatcher directoryWatcher_;
    QTimer refreshTimer_;
    QTimer thumbnailsTimer_;
    QTimer statusTimer_;
    QString directory_;
    QString pendingSelection_;
    ImageDocument document_;
    QSize displayedSize_;
    quint64 imageVersion_ = 0;
    std::shared_ptr<std::atomic<quint64>> loadGeneration_ = std::make_shared<std::atomic<quint64>>(0);
    double scaleFactor_ = 1;
    int controlLevel_ = -1;
    bool hasDetails_ = false;
    bool canvasHovered_ = false;
    bool busy_ = false;
    bool loading_ = false;
    bool scanning_ = false;
    bool animationSource_ = false;
    bool resumeMovie_ = false;
    bool restoreMaximized_ = false;
    QAction* fullscreenAction_ = nullptr;
    QAction* exitFullscreenAction_ = nullptr;
    QAction* sidebarAction_ = nullptr;
    QAction* screenshotDirectoryAction_ = nullptr;
    QAction* previousAction_ = nullptr;
    QAction* nextAction_ = nullptr;
    QAction* fitAction_ = nullptr;
    QAction* actualAction_ = nullptr;
    QAction* zoomInAction_ = nullptr;
    QAction* zoomOutAction_ = nullptr;
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
    QAction* wallpaperAction_ = nullptr;
    QAction* lockScreenAction_ = nullptr;
    QList<QAction*> editActions_;
    QList<QAction*> navigationActions_;
};
} // namespace ImageBrowser
