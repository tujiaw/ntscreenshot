#include "modules/image_browser/ImageDocument.h"
#include "modules/image_browser/ImageDirectoryModel.h"
#include "modules/image_browser/ImageLoader.h"
#include "modules/image_browser/ImageCanvas.h"
#include "modules/image_browser/ImageBrowserWindow.h"
#include "core/platform/Util.h"
#include "core/settings/SettingModel.h"
#include "core/theme/ThemeManager.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDir>
#include <QDebug>
#include <QEventLoop>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QImageReader>
#include <QKeyEvent>
#include <QListView>
#include <QLineF>
#include <QMessageBox>
#include <QMimeData>
#include <QMenu>
#include <QMouseEvent>
#include <QMovie>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTranslator>
#include <QWheelEvent>
#include <functional>
#ifdef Q_OS_WIN
#include <psapi.h>
#endif

using namespace ImageBrowser;
namespace {
int failures = 0;
void check(bool value, const char* name) {
    if (!value) { ++failures; qCritical() << "FAIL:" << name; }
}
bool waitUntil(const std::function<bool()>& condition, int timeout = 10000) {
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeout) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(5);
    }
    return condition();
}
QAction* action(QWidget* window, const QString& text) {
    for (auto* a : window->findChildren<QAction*>()) if (a->text() == text) return a;
    return nullptr;
}
void trigger(QWidget* window, const QString& text) {
    auto* a = action(window, text);
    check(a && a->isEnabled(), qPrintable(text));
    if (a && a->isEnabled()) a->trigger();
}
void mouse(QWidget* widget, QEvent::Type type, QPointF point) {
    QMouseEvent event(type, point, widget->mapToGlobal(point.toPoint()),
                      type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
                      type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(widget, &event);
}
void drop(QWidget* window, const QList<QUrl>& urls) {
    QMimeData data;
    data.setUrls(urls);
    QDragEnterEvent enter(QPoint(40, 40), Qt::CopyAction, &data, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(window, &enter);
    QDropEvent event(QPointF(40, 40), Qt::CopyAction, &data, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(window, &event);
    check(event.isAccepted(), "drop accepted");
}
void cancelPrompt() {
    QTimer::singleShot(0, [] {
        if (auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
            dialog->button(QMessageBox::Cancel)->click();
    });
}
void discardPrompt() {
    QTimer::singleShot(0, [] {
        if (auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
            for (auto* button : dialog->buttons()) {
                if (dialog->buttonRole(button) == QMessageBox::DestructiveRole) { button->click(); break; }
            }
        }
    });
}
qint64 privateBytes() {
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS_EX counters{};
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)))
        return counters.PrivateUsage;
#endif
    return 0;
}
}

int runImageBrowserTest(QApplication& app) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
#ifdef Q_OS_WIN
    const QDir fonts(QDir(qEnvironmentVariable("WINDIR", "C:/Windows")).filePath("Fonts"));
    QFontDatabase::addApplicationFont(fonts.filePath("msyh.ttc"));
    QFontDatabase::addApplicationFont(fonts.filePath("segoeui.ttf"));
    app.setFont(QFont(QStringLiteral("Microsoft YaHei"), 10));
#endif
    ThemeManager::apply(AppTheme::Light);
    QTemporaryDir temp;
    check(temp.isValid(), "temporary directory");
    QDir directory(temp.path());
    directory.mkdir("empty");
    directory.mkdir("nested");
    QImage source(320, 180, QImage::Format_ARGB32);
    source.fill(qRgba(12, 80, 160, 255));
    for (int y = 0; y < 90; ++y) for (int x = 0; x < 160; ++x) source.setPixel(x, y, qRgba(220, 30, 80, 128));
    source.setPixel(319, 179, qRgba(80, 170, 30, 0));
    const QString path = directory.filePath(QStringLiteral("测试图片.png"));
    check(source.save(path), "save source fixture");
    check(source.save(directory.filePath("other.png")), "save second fixture");
    check(source.save(directory.filePath("nested/hidden.png")), "nested fixture");
    QFile broken(directory.filePath("broken.png"));
    broken.open(QIODevice::WriteOnly); broken.write("broken"); broken.close();
    QFile ignored(directory.filePath("note.txt"));
    ignored.open(QIODevice::WriteOnly); ignored.write("plain text"); ignored.close();

    // Document invariants, geometric transforms, alpha, and command-only history.
    const auto loaded = ImageDocument::read(path);
    check(loaded.image == source && !loaded.limited, "decode with alpha and unicode path");
    ImageDocument document;
    document.load(path, loaded);
    check(document.sourceUnchanged() && !document.dirty(), "source baseline");
    document.append({EditOperation::RotateRight, {}, {}});
    auto result = ImageDocument::render(document.original, document.operations, document.position);
    check(result.image.size() == QSize(180, 320) && result.image.pixel(179, 0) == source.pixel(0, 0), "rotate right maps pixels");
    document.append({EditOperation::RotateLeft, {}, {}});
    result = ImageDocument::render(document.original, document.operations, document.position);
    check(result.image == source, "inverse rotation preserves alpha");
    document.append({EditOperation::FlipHorizontal, {}, {}});
    result = ImageDocument::render(document.original, document.operations, document.position);
    check(result.image.pixel(0, 179) == source.pixel(319, 179), "horizontal flip");
    document.append({EditOperation::FlipVertical, {}, {}});
    result = ImageDocument::render(document.original, document.operations, document.position);
    check(result.image.pixel(0, 0) == source.pixel(319, 179), "vertical flip");
    document.position = 0;
    result = ImageDocument::render(document.original, document.operations, document.position);
    check(result.image == source && !document.dirty() && document.canRedo(), "restore without snapshots");
    document.append({EditOperation::Crop, QRect(10, 20, 70, 40), {}});
    check(document.operations.size() == 1 && document.position == 1, "new edit discards redo branch");
    result = ImageDocument::render(document.original, document.operations, document.position);
    check(result.image == source.copy(10, 20, 70, 40), "crop pixels");
    --document.position;
    check(ImageDocument::render(document.original, document.operations, document.position).image == source, "undo crop");
    ++document.position;
    check(ImageDocument::render(document.original, document.operations, document.position).image.size() == QSize(70, 40), "redo crop");
    result = ImageDocument::render(source, {{EditOperation::Resize, {}, QSize(160, 90)}}, 1);
    check(result.image.size() == QSize(160, 90) && result.image.hasAlphaChannel() && qAlpha(result.image.pixel(0, 0)) == 128, "resize preserves alpha");
    check(!ImageDocument::render(source, {{EditOperation::Crop, QRect(-1, 0, 2, 2), {}}}, 1).error.isEmpty(), "reject out of bounds crop");
    check(!ImageDocument::render(source, {{EditOperation::Resize, {}, QSize(10000, 10000)}}, 1).error.isEmpty(), "resize pixel budget");
    const QString savedPath = ImageDocument::suggestedPath(path);
    check(ImageDocument::save(savedPath, source, "png").success, "atomic PNG save");
    check(ImageDocument::read(savedPath).image == source, "PNG round trip");
    check(ImageDocument::suggestedPath(path) != savedPath, "suggested filename avoids collisions");
    const auto before = ImageDocument::read(savedPath);
    check(!ImageDocument::save(savedPath, source, "invalid").success && ImageDocument::read(savedPath).image == before.image, "failed save preserves existing file");
    check(!ImageDocument::save(directory.filePath("missing/a.png"), source, "png").success, "save failure in missing directory");
    check(!ImageDocument::save(savedPath, source, "png", before.modified.addSecs(-1), before.fileSize).success, "reject source changed before commit");
    check(ImageDocument::read(savedPath).image == before.image, "source conflict preserves original");
    QImage transparent(20, 20, QImage::Format_ARGB32); transparent.fill(Qt::transparent);
    const QString jpeg = directory.filePath("white.jpg");
    check(ImageDocument::save(jpeg, transparent, "jpeg").success, "JPEG save");
    check(qRed(ImageDocument::read(jpeg).image.pixel(10, 10)) > 250, "JPEG white alpha background");
    const QString bitmap = directory.filePath("nested/converted.bmp");
    check(ImageDocument::save(bitmap, transparent, "bmp").success, "BMP format conversion");
    check(ImageDocument::read(bitmap).image.pixelColor(10, 10) == QColor(Qt::white), "BMP white alpha background");
    check(ImageDocument::suggestedPath(path, "jpeg").endsWith("-edited.jpg"), "conversion suggests matching extension");

    // Two-frame GIF with red and blue pixels, no additional codec dependencies.
    const QString gif = directory.filePath("animation.gif");
    QFile gifFile(gif); gifFile.open(QIODevice::WriteOnly);
    gifFile.write(QByteArray::fromHex("47494638396101000100800000ff00000000ff21ff0b4e45545343415045322e30030100000021f904000a0000002c000000000100010000020244010021f904000a0000002c00000000010001000002024c01003b"));
    gifFile.close();
    check(ImageDocument::read(gif).animated, "detect animated GIF");
    check(ImageDocument::readFrame(gif, 0).image.pixelColor(0, 0) == QColor(Qt::red), "read first GIF frame");
    check(ImageDocument::readFrame(gif, 1).image.pixelColor(0, 0) == QColor(Qt::blue), "read second GIF frame");

    QString error;
    const auto files = ImageDirectoryModel::enumerate(temp.path(), &error);
    check(error.isEmpty() && files.size() == 6, "current-directory filtering includes broken images");
    for (int i = 1; i < files.size(); ++i)
        check(files[i-1].lastModified() >= files[i].lastModified(), "newest images first");
    ImageDirectoryModel::enumerate(directory.filePath("missing"), &error);
    check(!error.isEmpty(), "invalid directory has explicit error");
    ImageLoader loader;
    ImageDirectoryModel model(&loader);
    int scans = 0;
    QObject::connect(&model, &ImageDirectoryModel::scanned, &app, [&](const QString&) { ++scans; });
    model.scan(temp.path()); model.scan(directory.filePath("empty"));
    check(waitUntil([&] { return scans == 1; }) && model.rowCount() == 0, "stale directory result discarded");
    model.scan(temp.path());
    check(waitUntil([&] { return scans == 2; }) && model.rowCount() == 6, "asynchronous scan");
    model.requestRange(0, 5, QSize(160, 100));
    check(waitUntil([&] { return model.cacheBytes() > 0; }) && model.cacheBytes() <= 64 * 1024 * 1024, "bounded thumbnail cache");
    {
        auto* closingModel = new ImageDirectoryModel(&loader);
        closingModel->scan(temp.path());
        delete closingModel;
    }

    // Coordinate mapping after zoom and pan, including crop movement and handles.
    ImageCanvas canvas;
    canvas.resize(640, 480); canvas.show(); canvas.setImage(source, true);
    check(canvas.zoom() == 1.0, "small images default to actual size");
    canvas.resize(240, 140);
    QCoreApplication::processEvents();
    check(canvas.zoom() < 1 && canvas.viewportPoint(QPointF(0, 0)).x() >= 16 &&
          canvas.viewportPoint(QPointF(0, 0)).y() >= 16, "large images fit with margins");
    canvas.actualSize();
    const QImage bordered = canvas.grab().toImage();
    check(bordered.pixelColor(2, 70) == ThemeManager::tokens().canvas, "zoomed canvas preserves blank border");
    canvas.resize(640, 480); canvas.setImage(source, true);
    const QPointF anchor(250, 200);
    const QPointF mappedBefore = canvas.imagePoint(anchor);
    QWheelEvent wheel(anchor, canvas.mapToGlobal(anchor.toPoint()), {}, QPoint(0, 120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&canvas, &wheel);
    check(QLineF(mappedBefore, canvas.imagePoint(anchor)).length() < 0.001, "zoom keeps cursor image point fixed");
    mouse(&canvas, QEvent::MouseButtonPress, QPointF(200, 200));
    mouse(&canvas, QEvent::MouseMove, QPointF(230, 225));
    mouse(&canvas, QEvent::MouseButtonRelease, QPointF(230, 225));
    canvas.setCropping(true);
    mouse(&canvas, QEvent::MouseButtonPress, canvas.viewportPoint(QPointF(10, 20)));
    mouse(&canvas, QEvent::MouseMove, canvas.viewportPoint(QPointF(80, 60)));
    mouse(&canvas, QEvent::MouseButtonRelease, canvas.viewportPoint(QPointF(80, 60)));
    check(canvas.cropRect() == QRect(10, 20, 70, 40), "crop maps zoomed and panned coordinates");
    mouse(&canvas, QEvent::MouseButtonPress, canvas.viewportPoint(QPointF(40, 40)));
    mouse(&canvas, QEvent::MouseMove, canvas.viewportPoint(QPointF(50, 50)));
    mouse(&canvas, QEvent::MouseButtonRelease, canvas.viewportPoint(QPointF(50, 50)));
    check(canvas.cropRect() == QRect(20, 30, 70, 40), "move crop area");
    mouse(&canvas, QEvent::MouseButtonPress, canvas.viewportPoint(QPointF(90, 70)));
    mouse(&canvas, QEvent::MouseMove, canvas.viewportPoint(QPointF(100, 80)));
    mouse(&canvas, QEvent::MouseButtonRelease, canvas.viewportPoint(QPointF(100, 80)));
    check(canvas.cropRect() == QRect(20, 30, 80, 50), "resize crop handle");
    canvas.hide();

    // Seed only the dedicated test application's settings, avoiding startup registry changes.
    QSettings testSettings(Util::getConfigPath(), QSettings::IniFormat);
    testSettings.setValue("UI_LANGUAGE", "zh"); testSettings.sync();
    SettingModel settings(nullptr);
    settings.setAutoSaveImage(false, temp.path());
    QPointer<ImageBrowserWindow> window = new ImageBrowserWindow(&settings, &loader);
    window->show();
    auto* list = window->findChild<QListView*>();
    check(waitUntil([&] { return list->isEnabled() && list->model()->rowCount() == 6; }), "default screenshot directory with autosave disabled");
    drop(window, {QUrl::fromLocalFile(path), QUrl::fromLocalFile(jpeg)});
    check(waitUntil([&] { auto* a = action(window, QStringLiteral("向右旋转")); return a && a->isEnabled(); }), "drop selects image");
    check(window->windowTitle().startsWith(QFileInfo(path).fileName()), "dropped file selected");
    QTimer::singleShot(0, [&] {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        check(menu && menu->actions().contains(action(window, QStringLiteral("格式转换"))) &&
              menu->actions().contains(action(window, QStringLiteral("覆盖原图"))) &&
              menu->actions().contains(action(window, QStringLiteral("选择目录"))), "context menu shares editing and browsing actions");
        if (menu) menu->close();
    });
    QContextMenuEvent context(QContextMenuEvent::Mouse, QPoint(40, 40), window->mapToGlobal(QPoint(40, 40)));
    QApplication::sendEvent(window, &context);
    QTimer::singleShot(0, [] {
        if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
            auto* formats = dialog->findChild<QComboBox*>();
            check(formats && formats->findData(QByteArray("png")) >= 0 && formats->findData(QByteArray("bmp")) >= 0,
                  "converter offers installed output formats");
            dialog->reject();
        }
    });
    trigger(window, QStringLiteral("格式转换"));
    check(!window->windowTitle().contains('*'), "cancel conversion leaves document unchanged");
    QTimer::singleShot(0, [] {
        if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
            const auto inputs = dialog->findChildren<QSpinBox*>();
            check(inputs.size() == 2, "resize dialog dimensions");
            if (inputs.size() == 2) {
                inputs[0]->setValue(160);
                check(inputs[1]->value() == 90, "resize dialog locks aspect ratio");
            }
            dialog->reject();
        }
    });
    trigger(window, QStringLiteral("调整图片尺寸"));
    check(!window->windowTitle().contains('*'), "cancel resize leaves image unchanged");
    trigger(window, QStringLiteral("向右旋转"));
    check(waitUntil([&] { return action(window, QStringLiteral("撤销"))->isEnabled(); }) && window->windowTitle().contains('*'), "edit marks document dirty");
    cancelPrompt();
    list->setCurrentIndex(list->model()->index(list->currentIndex().row() == 0 ? 1 : 0, 0));
    check(window && window->windowTitle().startsWith(QFileInfo(path).fileName()) && window->windowTitle().contains('*'), "cancel navigation retains edited image");
    trigger(window, QStringLiteral("撤销"));
    check(waitUntil([&] { return !window->windowTitle().contains('*'); }), "undo returns to clean image");
    trigger(window, QStringLiteral("重做"));
    check(waitUntil([&] { return window->windowTitle().contains('*') && action(window, QStringLiteral("另存为"))->isEnabled(); }), "redo restores edit");
    QTimer::singleShot(0, [] {
        if (auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget())) dialog->reject();
    });
    trigger(window, QStringLiteral("另存为"));
    check(window->windowTitle().contains('*'), "cancel save retains edit history");
    const QString uiSave = directory.filePath("ui-saved.png");
    QTimer::singleShot(0, [uiSave] {
        if (auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget())) {
            dialog->selectFile(uiSave);
            QMetaObject::invokeMethod(dialog, "accept", Qt::QueuedConnection);
        }
    });
    trigger(window, QStringLiteral("另存为"));
    check(waitUntil([&] { return QFileInfo::exists(uiSave) && !window->windowTitle().contains('*'); }), "UI async save establishes clean baseline");
    check(ImageDocument::read(uiSave).image.size() == QSize(180, 320), "UI saves full edit resolution");
    check(waitUntil([&] { return list->currentIndex().data(Qt::ToolTipRole).toString() == uiSave; }), "saved file selected after refresh");
    check(waitUntil([&] { return action(window, QStringLiteral("格式转换"))->isEnabled(); }), "saved image reload complete");
    const QString uiConverted = directory.filePath("nested/ui-converted.bmp");
    QTimer conversionSteps;
    conversionSteps.setInterval(10);
    QObject::connect(&conversionSteps, &QTimer::timeout, &app, [&] {
        auto* modal = QApplication::activeModalWidget();
        if (auto* fileDialog = qobject_cast<QFileDialog*>(modal)) {
            fileDialog->selectFile(uiConverted);
            QMetaObject::invokeMethod(fileDialog, "accept", Qt::QueuedConnection);
        } else if (auto* warning = qobject_cast<QMessageBox*>(modal)) {
            if (warning->button(QMessageBox::Yes)) warning->button(QMessageBox::Yes)->click();
        } else if (auto* dialog = qobject_cast<QDialog*>(modal)) {
            if (auto* formats = dialog->findChild<QComboBox*>()) {
                formats->setCurrentIndex(formats->findData(QByteArray("bmp")));
                dialog->accept();
            }
        }
    });
    conversionSteps.start();
    trigger(window, QStringLiteral("格式转换"));
    conversionSteps.stop();
    check(waitUntil([&] { return action(window, QStringLiteral("格式转换"))->isEnabled(); }), "converted file loaded");
    check(QFileInfo::exists(uiConverted) && ImageDocument::read(uiConverted).image.size() == QSize(180, 320), "converter saves full resolution output");
    check(window->windowTitle().startsWith("ui-converted.bmp") && !window->windowTitle().contains('*'), "conversion establishes new baseline");
    drop(window, {QUrl::fromLocalFile(uiSave)});
    check(waitUntil([&] { return action(window, QStringLiteral("向左旋转"))->isEnabled(); }), "return to source directory after conversion");
    trigger(window, QStringLiteral("向左旋转"));
    check(waitUntil([&] { return action(window, QStringLiteral("撤销"))->isEnabled(); }), "edit before watcher refresh");
    check(source.save(directory.filePath("new-screenshot.png")), "add screenshot while editing");
    check(waitUntil([&] { return list->model()->rowCount() == 8; }), "directory watcher adds new screenshot");
    check(window->windowTitle().startsWith("ui-saved.png") && window->windowTitle().contains('*'), "watcher does not steal selection or edits");
    cancelPrompt();
    window->close();
    check(window && window->isVisible(), "cancel close retains window");
    discardPrompt();
    drop(window, {QUrl::fromLocalFile(gif)});
    check(waitUntil([&] { return action(window, QStringLiteral("播放／暂停"))->isVisible(); }), "GIF opens playback controls");
    window->hide();
    check(window->findChildren<QMovie*>().last()->state() == QMovie::Paused, "hidden window pauses GIF");
    window->show();
    trigger(window, QStringLiteral("播放／暂停"));
    const auto movies = window->findChildren<QMovie*>();
    check(!movies.isEmpty() && movies.last()->state() == QMovie::Paused, "pause GIF");
    trigger(window, QStringLiteral("编辑当前帧"));
    check(waitUntil([&] { return action(window, QStringLiteral("裁剪"))->isEnabled(); }), "freeze full resolution GIF frame");
    check(!action(window, QStringLiteral("覆盖原图"))->isEnabled(), "animated source cannot be overwritten");
    discardPrompt();
    window->close();
    check(waitUntil([&] { return window.isNull(); }), "close releases window");

    const QString screenshots = qEnvironmentVariable("NTSCREENSHOT_IMAGE_BROWSER_QA_DIR");
    if (!screenshots.isEmpty()) {
        QDir().mkpath(screenshots);
        // A large fixture makes zoom, alpha, and thumbnail proportions inspectable.
        QImage fixture(1600, 900, QImage::Format_ARGB32);
        fixture.fill(QColor(244, 248, 250));
        QPainter painter(&fixture);
        painter.fillRect(QRect(0, 0, 1600, 120), QColor(34, 110, 90));
        painter.setPen(Qt::white); QFont font; font.setPixelSize(48); painter.setFont(font);
        painter.drawText(QRect(55, 20, 1450, 85), Qt::AlignVCenter, QStringLiteral("Screenshot workspace"));
        painter.setPen(QColor(40, 48, 52)); font.setPixelSize(30); painter.setFont(font);
        painter.drawText(70, 200, QStringLiteral("Image browser / crop / rotate / resize"));
        for (int i = 0; i < 6; ++i) {
            painter.fillRect(QRect(70 + i * 245, 270, 210, 300), QColor::fromHsv(i * 48, 150, 210));
            painter.drawText(70 + i * 245, 630, QStringLiteral("%1 x 900").arg(1600));
        }
        painter.end();
        const QString fixturePath = directory.filePath("workspace.png"); fixture.save(fixturePath);
        QPointer<ImageBrowserWindow> qa = new ImageBrowserWindow(&settings, &loader);
        qa->show(); drop(qa, {QUrl::fromLocalFile(fixturePath)});
        check(waitUntil([&] { return action(qa, QStringLiteral("裁剪"))->isEnabled(); }), "QA fixture loaded");
        for (const auto theme : {AppTheme::Light, AppTheme::Dark}) {
            ThemeManager::apply(theme);
            for (double scale : {1.0, 1.5, 2.0}) {
                check(QMetaObject::invokeMethod(qa, "applyDpi", Q_ARG(double, scale)), "apply window DPI");
                for (const QSize size : {QSize(1100, 740), QSize(560, 420)}) {
                    qa->resize(qRound(size.width() * scale), qRound(size.height() * scale));
                    QCoreApplication::processEvents();
                    QElapsedTimer settle; settle.start();
                    check(waitUntil([&] {
                        auto* images = qa->findChild<QListView*>();
                        return settle.elapsed() >= 100 && !qvariant_cast<QPixmap>(images->model()->index(0, 0).data(Qt::DecorationRole)).isNull();
                    }), "QA thumbnails loaded");
                    const auto* bar = qa->findChild<QToolBar*>();
                    for (auto* button : bar->findChildren<QToolButton*>()) {
                        if (button->isVisible()) check(bar->rect().contains(button->geometry()), "toolbar button fits at DPI");
                    }
                    auto* images = qa->findChild<QListView*>();
                    check(images->sizeHintForRow(0) >= images->fontMetrics().height() + qRound(80 * scale), "thumbnail text fits at DPI");
                    qa->grab().save(QDir(screenshots).filePath(QStringLiteral("browser-%1-%2-%3.png")
                        .arg(theme == AppTheme::Dark ? "dark" : "light").arg(size.width()).arg(qRound(scale * 100))));
                }
            }
        }
        qa->close(); waitUntil([&] { return qa.isNull(); });
    }
    if (qEnvironmentVariableIsSet("NTSCREENSHOT_IMAGE_BROWSER_BENCHMARK")) {
        directory.mkdir("many");
        const QString many = directory.filePath("many");
        for (int i = 0; i < 10000; ++i) QFile::copy(path, QDir(many).filePath(QStringLiteral("%1.png").arg(i, 5, 10, QChar('0'))));
        QElapsedTimer timer; timer.start();
        check(ImageDirectoryModel::enumerate(many).size() == 10000, "ten thousand image scan");
        qInfo() << "10,000 image enumeration ms:" << timer.elapsed();
        settings.setAutoSaveImage(false, many);
        QPointer<ImageBrowserWindow> largeWindow = new ImageBrowserWindow(&settings, &loader);
        largeWindow->show();
        int heartbeats = 0;
        QTimer heartbeat;
        heartbeat.setInterval(10);
        QObject::connect(&heartbeat, &QTimer::timeout, &app, [&] { ++heartbeats; });
        heartbeat.start(); timer.restart();
        check(waitUntil([&] {
            auto* images = largeWindow->findChild<QListView*>();
            return images->model()->rowCount() == 10000 && action(largeWindow, QStringLiteral("裁剪"))->isEnabled();
        }), "ten thousand image first view");
        heartbeat.stop();
        qInfo() << "10,000 image first-view ms:" << timer.elapsed() << "UI heartbeats:" << heartbeats;
        check(heartbeats > 0, "directory scan keeps event loop responsive");
        largeWindow->close(); waitUntil([&] { return largeWindow.isNull(); });
        {
            QImage huge(8000, 5001, QImage::Format_RGB32); huge.fill(Qt::white);
            const QString hugePath = directory.filePath("huge.png"); huge.save(hugePath);
            huge = {};
            const auto preview = ImageDocument::read(hugePath);
            check(preview.limited && !preview.image.isNull() && preview.image.width() <= 2048, "oversized image bounded preview");
        }
        {
            QImage longImage(128, 30000, QImage::Format_RGB32); longImage.fill(Qt::white);
            const QString longPath = directory.filePath("long.png"); longImage.save(longPath);
            check(ImageDocument::read(longPath).image.size() == longImage.size(), "long screenshot retains full resolution");
        }
        QList<qint64> memory;
        for (int i = 0; i < 10; ++i) {
            QPointer<ImageBrowserWindow> repeated = new ImageBrowserWindow(&settings, &loader);
            repeated->show();
            repeated->close();
            check(waitUntil([&] { return repeated.isNull(); }), "close during directory load");
            memory.append(privateBytes() / (1024 * 1024));
        }
        qInfo() << "Repeated open/close private memory MiB:" << memory;
    }
    QTranslator english;
    check(english.load(QStringLiteral(":/i18n/ntscreenshot_en.qm")), "load English translations");
    app.installTranslator(&english);
    check(QCoreApplication::translate("ImageBrowser::ImageBrowserWindow", "图片浏览") == "Image browser", "browser English translation");
    app.removeTranslator(&english);
    qInfo() << "Image browser test failures:" << failures;
    return failures ? 1 : 0;
}
