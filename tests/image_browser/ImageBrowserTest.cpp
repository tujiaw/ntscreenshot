#include "modules/image_browser/ImageDocument.h"
#include "modules/image_browser/ImageMosaic.h"
#include "modules/image_browser/ImageEditorPanel.h"
#include "core/imaging/CvBridge.h"
#include "modules/image_browser/ImageDirectoryModel.h"
#include "modules/image_browser/ImageLoader.h"
#include "modules/image_browser/ImageCanvas.h"
#include "modules/image_browser/ImageBrowserWindow.h"
#include "modules/image_browser/ImageBrowserModule.h"
#include "modules/image_browser/ImageConversionDialog.h"
#include "core/platform/Util.h"
#include "core/platform/SystemBackground.h"
#include "core/settings/SettingModel.h"
#include "core/theme/ThemeManager.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCheckBox>
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
#include <QLabel>
#include <QListView>
#include <QLocale>
#include <QLineF>
#include <QLineEdit>
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
#include <QStackedWidget>
#include <QScrollArea>
#include <QStatusBar>
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
    auto ready = [&] {
        if (!condition()) return false;
        for (auto* widget : QApplication::topLevelWidgets())
            if (widget->property("imageProcessing").toBool()) return false;
        return true;
    };
    QElapsedTimer timer;
    timer.start();
    while (!ready() && timer.elapsed() < timeout) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(5);
    }
    return ready();
}
QAction* action(QWidget* window, const QString& text) {
    for (auto* a : window->findChildren<QAction*>()) if (a->text() == text) return a;
    return nullptr;
}
void trigger(QWidget* window, const QString& text) {
    auto* a = action(window, text);
    check(a && a->isEnabled(), qPrintable(text));
    if (a && a->isEnabled()) a->trigger();
    check(waitUntil([] { return true; }), "image processing completes before checking action result");
}
class SurfaceStabilityProbe final : public QObject {
public:
    explicit SurfaceStabilityProbe(QWidget* window) {
        const QStringList names{QStringLiteral("编辑"), QStringLiteral("格式转换"), QStringLiteral("刷新"),
            QStringLiteral("复制图片"), QStringLiteral("另存为"), QStringLiteral("适应窗口"), QStringLiteral("实际大小"), QStringLiteral("全屏")};
        for (auto* a : window->findChildren<QAction*>()) if (names.contains(a->text())) {
            icons.append({a, a->icon().cacheKey()});
            connect(a, &QAction::changed, this, [this] { ++actionChanges; });
        }
        for (auto* button : window->findChildren<QToolButton*>()) if (button->defaultAction() && names.contains(button->defaultAction()->text())) {
            surfaces.append({button, button->geometry()}); button->installEventFilter(this);
        }
        for (auto* toolbar : window->findChildren<QToolBar*>()) toolbar->installEventFilter(this);
    }
    bool stable() const {
        if (actionChanges || surfaceEvents) return false;
        for (const auto& entry : icons) if (!entry.first || entry.first->icon().cacheKey() != entry.second) return false;
        for (const auto& entry : surfaces) if (!entry.first || entry.first->geometry() != entry.second) return false;
        return true;
    }
    int actionChanges = 0, surfaceEvents = 0;
protected:
    bool eventFilter(QObject*, QEvent* event) override {
        if (event->type() == QEvent::StyleChange || event->type() == QEvent::EnabledChange ||
            event->type() == QEvent::Show || event->type() == QEvent::Hide) ++surfaceEvents;
        return false;
    }
private:
    QList<QPair<QPointer<QAction>, qint64>> icons;
    QList<QPair<QPointer<QWidget>, QRect>> surfaces;
};
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
    const QString backgroundCache = directory.filePath(QStringLiteral("背景缓存"));
    const auto background = SystemBackground::prepare(source, backgroundCache);
    const QImage backgroundImage(background.path);
    check(background.success && backgroundImage.size() == source.size() && !backgroundImage.hasAlphaChannel(), "background snapshot is a persistent opaque image");
    QImage transparentBackground(16, 16, QImage::Format_ARGB32); transparentBackground.fill(Qt::transparent);
    const auto whiteBackground = SystemBackground::prepare(transparentBackground, backgroundCache);
    check(whiteBackground.success && QImage(whiteBackground.path).pixelColor(8, 8) == QColor(Qt::white), "transparent background area flattened to white");
    check(SystemBackground::prepare(source, backgroundCache).path == background.path, "same image has a stable background snapshot path");
    QImage changedBackground = source; changedBackground.fill(Qt::green);
    check(SystemBackground::prepare(changedBackground, backgroundCache).path != background.path, "edited image gets its own background snapshot");
    check(!SystemBackground::prepare({}, backgroundCache).success, "empty background is rejected");
    check(!SystemBackground::prepare(source, path).success, "background cache file conflict is reported");
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
    check(!ImageDocument::render(source, {{EditOperation::Resize, {}, QSize(9000, 8001)}}, 1).error.isEmpty(), "resize pixel budget");
    QImage brushSource(512, 320, QImage::Format_ARGB32);
    for (int y = 0; y < brushSource.height(); ++y) for (int x = 0; x < brushSource.width(); ++x)
        brushSource.setPixel(x, y, qRgba(x%256, y%256, (x+y)%256, 128));
    EditOperation brush; brush.kind = EditOperation::Mosaic; brush.directDrawing = true;
    brush.brushDiameter = 30; brush.strength = 12;
    brush.brushPoints = {QPointF(30, 70), QPointF(430, 70), QPointF(430, 210)};
    for (int style = 0; style < 3; ++style) {
        brush.variant = style; brush.color = QColor("#102030");
        const auto brushed = ImageDocument::render(brushSource, {brush}, 1);
        check(brushed.error.isEmpty() && brushed.image.size() == brushSource.size(), "all mosaic styles render at original size");
        check(brushed.image.pixel(200, 100) == brushSource.pixel(200, 100) &&
            brushed.image.pixel(15, 55) == brushSource.pixel(15, 55), "circular brush preserves pixels outside stroke");
        check(style == 2 ? brushed.image.pixelColor(200, 70) == brush.color : qAlpha(brushed.image.pixel(200, 70)) == 128,
            "mosaic preserves alpha while solid cover uses chosen color");
        QImage incremental = brushSource;
        for (int i = 0; i < brush.brushPoints.size(); ++i) {
            EditOperation segment = brush;
            segment.brushPoints = i ? QPolygonF{brush.brushPoints[i-1], brush.brushPoints[i]} : QPolygonF{brush.brushPoints[i]};
            QPainter painter(&incremental); paintMosaic(painter, brushSource, segment);
        }
        check(incremental == brushed.image, "incremental brush preview matches committed path exactly");
    }
    brush.variant = 0;
    const auto pixelBrush = ImageDocument::render(brushSource, {brush}, 1).image;
    check(pixelBrush.pixel(200, 70) == brushSource.pixel(198, 66), "pixel brush uses stable image-aligned blocks");
    brush.brushPoints = {QPointF(30, 70)}; brush.variant = 2;
    check(ImageDocument::render(brushSource, {brush}, 1).image.pixelColor(30, 70) == brush.color, "single brush click paints circular dot");
    brush.brushPoints.clear();
    check(!ImageDocument::render(brushSource, {brush}, 1).error.isEmpty(), "empty brush stroke is rejected");
    EditOperation filter; filter.kind = EditOperation::Filter;
    const auto gray = ImageDocument::render(source, {filter}, 1).image;
    check(qRed(gray.pixel(0, 0)) == qGreen(gray.pixel(0, 0)) && qAlpha(gray.pixel(0, 0)) == 128, "grayscale preserves alpha");
    filter.variant = 2;
    check(ImageDocument::render(source, {filter}, 1).image.pixel(0, 0) == qRgba(35, 225, 175, 128), "invert RGB without changing alpha");
    EditOperation color; color.kind = EditOperation::AdjustColor; color.saturation = -100;
    const auto desaturated = ImageDocument::render(source, {color}, 1).image;
    check(qRed(desaturated.pixel(0, 0)) == qBlue(desaturated.pixel(0, 0)) && qAlpha(desaturated.pixel(0, 0)) == 128, "color adjustment preserves alpha");
    EditOperation balance; balance.kind = EditOperation::AdjustColor;
    check(ImageDocument::render(source, {balance}, 1).image == source, "neutral color controls preserve exact pixels");
    balance.temperature = 50;
    auto balanced = ImageDocument::render(source, {balance}, 1).image.pixel(0, 0);
    check(qRed(balanced) == 250 && qBlue(balanced) == 50 && qGreen(balanced) == 30, "temperature warms red and cools blue");
    balance.temperature = 0; balance.tint = 50; balance.opacity = 50;
    balanced = ImageDocument::render(source, {balance}, 1).image.pixel(0, 0);
    check(qRed(balanced) == 235 && qGreen(balanced) == 15 && qBlue(balanced) == 95 && qAlpha(balanced) == 64, "tint and opacity affect actual pixels");
    for (int mode = 3; mode <= 8; ++mode) {
        EditOperation cvFilter; cvFilter.kind = EditOperation::Filter; cvFilter.variant = mode; cvFilter.strength = 3;
        const auto processed = ImageDocument::render(source, {cvFilter}, 1);
        check(processed.error.isEmpty() && processed.image.size() == source.size() && processed.image.pixelColor(0, 0).alpha() == 128,
            "OpenCV filters preserve original dimensions and alpha");
    }
    QImage tiledFixture(1100, 580, QImage::Format_ARGB32);
    for (int y = 0; y < tiledFixture.height(); ++y) for (int x = 0; x < tiledFixture.width(); ++x)
        tiledFixture.setPixel(x, y, qRgb((x*17+y)%256, (x+y*9)%256, (x*5+y*3)%256));
    EditOperation blur; blur.kind = EditOperation::Filter; blur.variant = 3; blur.strength = 3;
    cv::Mat fullBlur;
    cv::GaussianBlur(CvBridge::QImageToMat(tiledFixture), fullBlur, cv::Size(7, 7), 0);
    check(ImageDocument::render(tiledFixture, {blur}, 1).image == CvBridge::MatToQImage(fullBlur), "tiled OpenCV blur matches full-frame filtering without seams");
    QImage noisy(25, 25, QImage::Format_ARGB32); noisy.fill(Qt::white); noisy.setPixelColor(12, 12, Qt::black);
    EditOperation denoise = blur; denoise.variant = 4; denoise.strength = 1;
    check(ImageDocument::render(noisy, {denoise}, 1).image.pixelColor(12, 12) == QColor(Qt::white), "median denoise removes isolated noise");
    QImage lowContrast(3, 1, QImage::Format_ARGB32);
    lowContrast.setPixel(0, 0, qRgba(50, 50, 50, 128)); lowContrast.setPixel(1, 0, qRgb(100, 100, 100)); lowContrast.setPixel(2, 0, qRgb(150, 150, 150));
    EditOperation autoContrast = blur; autoContrast.variant = 8;
    const auto stretched = ImageDocument::render(lowContrast, {autoContrast}, 1).image;
    check(qRed(stretched.pixel(0, 0)) == 0 && qRed(stretched.pixel(2, 0)) == 255 && qAlpha(stretched.pixel(0, 0)) == 128, "automatic contrast stretches range and retains alpha");
    QImage cutoutSource(96, 96, QImage::Format_ARGB32); cutoutSource.fill(Qt::white);
    { QPainter subject(&cutoutSource); subject.setCompositionMode(QPainter::CompositionMode_Source);
      subject.setPen(Qt::NoPen); subject.setBrush(QColor(220, 30, 60, 128)); subject.drawEllipse(QRect(24, 16, 48, 64)); }
    const QString cutoutPath = directory.filePath("nested/cutout-source.png"); check(cutoutSource.save(cutoutPath), "cutout fixture saved");
    EditOperation cutout; cutout.kind = EditOperation::Cutout; cutout.rect = QRect(12, 8, 72, 80); cutout.strength = 2; cutout.feather = 0;
    const auto removed = ImageDocument::render(cutoutSource, {cutout}, 1);
    check(removed.error.isEmpty() && removed.image.size() == cutoutSource.size() && removed.image.pixelColor(0, 0).alpha() == 0 &&
        removed.image.pixelColor(48, 48).alpha() == 128, "GrabCut removes background and preserves subject transparency and resolution");
    const QString cutoutPng = directory.filePath("nested/cutout-result.png");
    check(ImageDocument::save(cutoutPng, removed.image, "png").success && QImage(cutoutPng) == removed.image, "cutout PNG round-trip retains alpha");
    check(ImageDocument::render(cutoutSource, {cutout}, 1).image == removed.image, "GrabCut replay is deterministic for undo and redo");
    EditOperation invalidCutout = cutout; invalidCutout.rect = cutoutSource.rect();
    check(!ImageDocument::render(cutoutSource, {invalidCutout}, 1).error.isEmpty(), "GrabCut rejects full-image selection without background samples");
    invalidCutout.rect = QRect(1, 1, 2, 2);
    check(!ImageDocument::render(cutoutSource, {invalidCutout}, 1).error.isEmpty(), "GrabCut rejects insufficient foreground samples without invoking clustering");
    QImage tinyCutout(3, 3, QImage::Format_ARGB32); tinyCutout.fill(Qt::white);
    invalidCutout.rect = QRect(0, 0, 3, 2);
    check(!ImageDocument::render(tinyCutout, {invalidCutout}, 1).error.isEmpty(), "GrabCut rejects insufficient background samples safely");
    cutout.feather = 2;
    const auto feathered = ImageDocument::render(cutoutSource, {cutout}, 1);
    bool softEdge = false;
    for (int y = 0; y < cutoutSource.height(); ++y) for (int x = 0; x < cutoutSource.width(); ++x)
        if (cutoutSource.pixelColor(x, y).alpha() == 255 && feathered.image.pixelColor(x, y).alpha() > 0 && feathered.image.pixelColor(x, y).alpha() < 255) softEdge = true;
    check(feathered.error.isEmpty() && softEdge, "cutout feather smooths the mask boundary");
    cutout.feather = 0;
    EditOperation mosaic; mosaic.kind = EditOperation::Mosaic; mosaic.rect = QRect(150, 80, 30, 30); mosaic.strength = 30;
    const auto pixelated = ImageDocument::render(source, {mosaic}, 1).image;
    check(pixelated.pixel(150, 80) != source.pixel(150, 80) &&
        (pixelated.pixel(150, 80) & 0x00ffffff) == (pixelated.pixel(179, 109) & 0x00ffffff) &&
        qAlpha(pixelated.pixel(150, 80)) == 128 && pixelated.pixel(0, 0) == source.pixel(0, 0), "mosaic affects selected RGB while preserving original alpha");
    EditOperation annotation; annotation.kind = EditOperation::Annotate; annotation.rect = QRect(20, 20, 70, 50);
    check(ImageDocument::render(source, {annotation}, 1).image != source, "rectangle annotation rendered");
    annotation.variant = 1;
    check(ImageDocument::render(source, {annotation}, 1).image != source, "ellipse annotation rendered");
    annotation.variant = 2;
    check(ImageDocument::render(source, {annotation}, 1).image != source, "arrow annotation rendered");
    EditOperation watermark; watermark.kind = EditOperation::Watermark; watermark.rect = source.rect(); watermark.text = "watermark"; watermark.strength = 30;
    check(ImageDocument::render(source, {watermark}, 1).image != source, "text watermark rendered");
    watermark.rect = {}; watermark.opacity = 45;
    for (int mode = 1; mode <= 6; ++mode) {
        watermark.variant = mode;
        const auto marked = ImageDocument::render(source, {watermark}, 1);
        check(marked.error.isEmpty() && marked.image != source && marked.image.size() == source.size(), "watermark preset needs no region");
    }
    watermark.opacity = 0;
    check(ImageDocument::render(source, {watermark}, 1).image == source, "transparent watermark preserves image");
    EditOperation sticker; sticker.kind = EditOperation::Sticker; sticker.rect = QRect(10, 10, 20, 20);
    sticker.asset = QImage(2, 2, QImage::Format_ARGB32); sticker.asset.fill(Qt::green);
    const auto pasted = ImageDocument::render(source, {sticker}, 1).image;
    check(pasted.pixel(15, 15) == qRgb(0, 255, 0) && pasted.pixel(0, 0) == source.pixel(0, 0), "sticker is composited inside selection");
    EditOperation perspective; perspective.kind = EditOperation::Perspective;
    perspective.corners << QPointF(0, 0) << QPointF(319, 0) << QPointF(319, 179) << QPointF(0, 179);
    perspective.size = source.size();
    const auto rectified = ImageDocument::render(source, {perspective}, 1);
    check(rectified.error.isEmpty() && rectified.image.size() == source.size() && qAlpha(rectified.image.pixel(0, 0)) == 128, "perspective output size and alpha");
    perspective.corners[1] = perspective.corners[0];
    check(!ImageDocument::render(source, {perspective}, 1).error.isEmpty(), "reject degenerate perspective corners");
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

    {
        // 72 MP crosses both the previous pixel cap and Qt's usual 256 MiB
        // decoder budget. Release the fixture before decoding to bound test memory.
        const QString largePath = directory.filePath("nested/72mp.png");
        QImage large(9000, 8000, QImage::Format_RGB32);
        check(!large.isNull(), "large image fixture allocation");
        large.fill(Qt::blue);
        check(large.save(largePath), "save 72 megapixel fixture");
        large = {};
        const auto decoded = ImageDocument::read(largePath);
        check(!decoded.limited && decoded.error.isEmpty() && decoded.image.size() == QSize(9000, 8000),
            "72 megapixel image decodes at full resolution");
        EditOperation smallCrop; smallCrop.kind = EditOperation::Crop; smallCrop.rect = QRect(0, 0, 32, 32);
        const auto cropped = ImageDocument::render(decoded.image, {smallCrop}, 1);
        check(cropped.error.isEmpty() && cropped.image.size() == QSize(32, 32) && cropped.image.pixelColor(10, 10) == QColor(Qt::blue),
            "image above previous limit can be edited");
    }
    {
        // A one-bit fixture exercises the upper boundary without allocating a
        // full 400 MiB RGBA source just to test the restricted-preview branch.
        const QString oversizedPath = directory.filePath("nested/over72mp.png");
        QImage oversized(9000, 8001, QImage::Format_Mono);
        oversized.setColorTable({qRgb(255, 255, 255), qRgb(0, 0, 0)}); oversized.fill(0);
        check(oversized.save(oversizedPath), "save image above new pixel cap");
        EditOperation invalidEdit; invalidEdit.kind = EditOperation::Crop; invalidEdit.rect = QRect(0, 0, 2, 2);
        check(!ImageDocument::render(oversized, {invalidEdit}, 1).error.isEmpty(), "document rendering also enforces the 72 megapixel budget");
        oversized = {};
        const auto preview = ImageDocument::read(oversizedPath);
        check(preview.limited && !preview.image.isNull() && preview.image.width() <= 2048 && preview.image.height() <= 2048,
            "images above 72 megapixels retain bounded preview");
    }

    auto contents = [](const QString& fileName) { QFile file(fileName); file.open(QIODevice::ReadOnly); return file.readAll(); };
    ImageConversionOptions outputOptions;
    outputOptions.outputSize = QSize(160, 90); outputOptions.grayscale = true;
    const QString grayPath = directory.filePath("nested/grayscale.png");
    check(ImageDocument::save(grayPath, source, "png", {}, -1, outputOptions).success, "resize and grayscale conversion");
    const QImage grayImage(grayPath);
    const QColor grayPixel = grayImage.pixelColor(10, 10);
    check(grayImage.size() == outputOptions.outputSize && grayPixel.red() == grayPixel.green() &&
        grayPixel.green() == grayPixel.blue() && grayPixel.alpha() == 128, "conversion size, grayscale and alpha preserved");
    outputOptions = {}; outputOptions.pngCompression = 0;
    const QString fastPng = directory.filePath("nested/fast.png");
    const QString smallPng = directory.filePath("nested/small.png");
    check(ImageDocument::save(fastPng, source, "png", {}, -1, outputOptions).success, "uncompressed PNG conversion");
    outputOptions.pngCompression = 100;
    check(ImageDocument::save(smallPng, source, "png", {}, -1, outputOptions).success &&
        QFileInfo(smallPng).size() < QFileInfo(fastPng).size() && QImage(smallPng) == source, "PNG compression changes size without losing pixels");
    const QString lowJpeg = directory.filePath("nested/low.jpg");
    const QString highJpeg = directory.filePath("nested/high.jpg");
    outputOptions = {}; outputOptions.quality = 10;
    check(ImageDocument::save(lowJpeg, source, "jpeg", {}, -1, outputOptions).success, "low JPEG quality conversion");
    outputOptions.quality = 100;
    check(ImageDocument::save(highJpeg, source, "jpeg", {}, -1, outputOptions).success &&
        QFileInfo(highJpeg).size() > QFileInfo(lowJpeg).size(), "JPEG quality changes encoded output");
    const QString icoPath = directory.filePath("nested/image.ico");
    for (int size : {16, 24, 32, 48, 64, 128, 256}) {
        outputOptions = {}; outputOptions.icoSize = size;
        check(ImageDocument::save(icoPath, source, "ico", {}, -1, outputOptions).success, "ICO encodes common icon sizes");
        const QImage icon(icoPath);
        check(contents(icoPath).startsWith(QByteArray::fromHex("000001000100")) && icon.size() == QSize(size, size), "ICO header and decoded square dimensions");
        check(icon.pixelColor(0, 0).alpha() == 0 && icon.pixelColor(size/2, size/2).alpha() > 0, "ICO centers proportional image with transparent padding");
    }
    const QByteArray iconData = contents(icoPath);
    outputOptions.icoSize = 257;
    check(!ImageDocument::save(icoPath, source, "ico", {}, -1, outputOptions).success && contents(icoPath) == iconData,
        "invalid ICO size preserves existing file");
    check(ImageDocument::suggestedPath(path, "ico").endsWith("-edited.ico"), "ICO suggested extension");
    const QString pdfPath = directory.filePath("nested/image.pdf");
    const QString a4Path = directory.filePath("nested/a4.pdf");
    check(ImageDocument::save(pdfPath, source, "pdf").success, "atomic PDF export");
    const QByteArray pdfData = contents(pdfPath);
    check(pdfData.startsWith("%PDF-") && pdfData.trimmed().endsWith("%%EOF") && pdfData.contains("/MediaBox"), "PDF has header, page and completed trailer");
    outputOptions = {}; outputOptions.pdfA4 = true; outputOptions.pdfOrientation = 2; outputOptions.pdfMarginMm = 12;
    check(ImageDocument::save(a4Path, source, "pdf", {}, -1, outputOptions).success, "landscape A4 PDF with margins");
    outputOptions.pdfMarginMm = 51;
    check(!ImageDocument::save(pdfPath, source, "pdf", {}, -1, outputOptions).success && contents(pdfPath) == pdfData,
        "failed PDF export preserves existing file");
    outputOptions = {}; outputOptions.outputSize = QSize(int(MaxEditablePixels), 2);
    check(!ImageDocument::save(grayPath, source, "png", {}, -1, outputOptions).success && QImage(grayPath) == grayImage,
        "oversized conversion rejected before replacing file");
    ImageConversionDialog conversionDialog(source.size());
    check(MaxEditablePixels == 72000000 && conversionDialog.findChild<QSpinBox*>("conversionWidth")->maximum() == MaxEditablePixels,
        "conversion and editing share the 72 megapixel limit");
    auto* conversionFormat = conversionDialog.findChild<QComboBox*>("conversionFormat");
    conversionFormat->setCurrentIndex(conversionFormat->findData(QByteArray("pdf")));
    check(!conversionDialog.findChild<QSpinBox*>("conversionQuality")->isVisibleTo(&conversionDialog) &&
        conversionDialog.findChild<QComboBox*>("conversionPaper")->isVisibleTo(&conversionDialog), "PDF exposes page settings instead of image quality");
    conversionDialog.findChild<QComboBox*>("conversionPaper")->setCurrentIndex(1);
    conversionDialog.findChild<QComboBox*>("conversionOrientation")->setCurrentIndex(2);
    conversionDialog.findChild<QSpinBox*>("conversionMargin")->setValue(12);
    conversionDialog.findChild<QCheckBox*>("conversionResize")->setChecked(true);
    conversionDialog.findChild<QSpinBox*>("conversionWidth")->setValue(160);
    check(conversionDialog.options().outputSize == QSize(160, 90) && conversionDialog.options().pdfA4 &&
        conversionDialog.options().pdfOrientation == 2 && conversionDialog.options().pdfMarginMm == 12,
        "conversion dialog collects settings and locks aspect ratio");
    check(conversionFormat->findData(QByteArray("ico")) >= 0, "converter exposes ICO");
    conversionFormat->setCurrentIndex(conversionFormat->findData(QByteArray("ico")));
    auto* iconSizes = conversionDialog.findChild<QComboBox*>("conversionIcoSize");
    iconSizes->setCurrentIndex(iconSizes->findData(64));
    check(iconSizes->isVisibleTo(&conversionDialog) && !conversionDialog.findChild<QSpinBox*>("conversionWidth")->isVisibleTo(&conversionDialog) &&
        conversionDialog.options().icoSize == 64 && conversionDialog.options().outputSize.isEmpty(), "ICO uses icon sizes and ignores hidden resize draft");
    conversionFormat->setCurrentIndex(conversionFormat->findData(QByteArray("pdf")));
    const QString conversionQa = qEnvironmentVariable("NTSCREENSHOT_CONVERSION_QA_DIR");
    if (!conversionQa.isEmpty()) {
        QDir().mkpath(conversionQa);
        QFile::copy(pdfPath, QDir(conversionQa).filePath("image.pdf"));
        QFile::copy(a4Path, QDir(conversionQa).filePath("a4.pdf"));
        source.save(QDir(conversionQa).filePath("source.png"));
        conversionDialog.show(); QCoreApplication::processEvents();
        conversionDialog.grab().save(QDir(conversionQa).filePath("conversion-pdf.png"));
        conversionFormat->setCurrentIndex(conversionFormat->findData(QByteArray("ico")));
        QCoreApplication::processEvents();
        conversionDialog.grab().save(QDir(conversionQa).filePath("conversion-ico.png"));
        QFile::copy(icoPath, QDir(conversionQa).filePath("image.ico"));
        conversionDialog.hide();
    }

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
#ifdef Q_OS_WIN
    if (qEnvironmentVariableIsSet("NTSCREENSHOT_IMAGE_BROWSER_STARTUP_ONLY")) {
        for (const auto target : {SystemBackground::Target::Desktop, SystemBackground::Target::LockScreen}) {
            bool completed = false;
            loader.run(&app, [target] { return SystemBackground::checkAvailability(target); },
                [&, target](const SystemBackground::Result& result) {
                    check(result.success, target == SystemBackground::Target::Desktop
                        ? "native desktop wallpaper COM activation and query"
                        : "native lock screen and storage WinRT activation");
                    if (!result.success) qCritical() << result.error;
                    completed = true;
                });
            if (!waitUntil([&] { return completed; }, 20000)) {
                qCritical() << "Native background service probe timed out";
                return 1;
            }
        }
    }
#endif
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
    {
        ImageCanvas brushCanvas; brushCanvas.resize(640, 480); brushCanvas.show();
        const QImage rgbSource = brushSource.convertToFormat(QImage::Format_RGB888);
        brushCanvas.setImage(rgbSource, true);
        EditOperation dot; dot.kind = EditOperation::Mosaic; dot.variant = 2;
        dot.color = QColor("#102030"); dot.brushDiameter = 30;
        brushCanvas.setAnnotation(dot, true);
        int accepted = 0; EditOperation paintedStroke;
        QObject::connect(&brushCanvas, &ImageCanvas::annotationAccepted, &brushCanvas,
            [&](const EditOperation& op) { ++accepted; paintedStroke = op; });
        mouse(&brushCanvas, QEvent::MouseButtonPress, brushCanvas.viewportPoint(QPointF(200, 70)));
        mouse(&brushCanvas, QEvent::MouseMove, brushCanvas.viewportPoint(QPointF(300, 70)));
        check(brushCanvas.cursor().shape() == Qt::BlankCursor, "mosaic uses drawn circular cursor on image");
        const QImage preview = brushCanvas.grab().toImage();
        check(preview.pixelColor(brushCanvas.viewportPoint(QPointF(250, 70)).toPoint()) == dot.color,
            "brush paints actual pixels during drag before release, including RGB888 input");
        mouse(&brushCanvas, QEvent::MouseButtonRelease, brushCanvas.viewportPoint(QPointF(300, 70)));
        check(accepted == 1 && paintedStroke.brushPoints.size() == 2, "whole drag commits as one brush operation");
        brushCanvas.setAnnotation(dot, false);
        check(brushCanvas.grab().toImage().pixelColor(brushCanvas.viewportPoint(QPointF(250, 70)).toPoint()) == dot.color,
            "painted preview remains visible while asynchronous commit disables brush");
        brushCanvas.setImage(rgbSource); brushCanvas.setAnnotation(dot, true);
        mouse(&brushCanvas, QEvent::MouseButtonPress, brushCanvas.viewportPoint(QPointF(200, 70)));
        check(brushCanvas.cancelDrawing(), "brush preview can be cancelled");
        mouse(&brushCanvas, QEvent::MouseButtonRelease, brushCanvas.viewportPoint(QPointF(300, 70)));
        check(accepted == 1, "cancelled brush does not enter history");
        for (const double zoom : {.25, .5, 1., 2., 4.}) {
            brushCanvas.setImage(rgbSource, true); brushCanvas.setZoom(zoom); brushCanvas.setAnnotation(dot, true);
            const QPointF center(rgbSource.width()/2., rgbSource.height()/2.);
            mouse(&brushCanvas, QEvent::MouseButtonPress, brushCanvas.viewportPoint(center));
            mouse(&brushCanvas, QEvent::MouseButtonRelease, brushCanvas.viewportPoint(center));
            check(qAbs(paintedStroke.brushDiameter*zoom-dot.brushDiameter) < .001,
                "mosaic brush diameter stays fixed in screen pixels across zoom levels");
        }
    }
    canvas.resize(640, 480); canvas.show(); canvas.setImage(source, true);
    check(canvas.zoom() == 1.0, "small images default to actual size");
    canvas.fit();
    check(canvas.zoom() > 1.0, "explicit fit can enlarge small images");
    canvas.setImage(source, true);
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
    canvas.setCropping(false);
    EditOperation stroke; stroke.kind = EditOperation::Annotate; stroke.variant = 2; stroke.strength = 4;
    int drawn = 0; EditOperation committed;
    QObject::connect(&canvas, &ImageCanvas::annotationAccepted, &canvas, [&](const EditOperation& op) { ++drawn; committed = op; });
    canvas.setAnnotation(stroke, true);
    mouse(&canvas, QEvent::MouseButtonPress, canvas.viewportPoint(QPointF(100, 60)));
    mouse(&canvas, QEvent::MouseMove, canvas.viewportPoint(QPointF(20, 60)));
    mouse(&canvas, QEvent::MouseButtonRelease, canvas.viewportPoint(QPointF(20, 60)));
    check(drawn == 1 && committed.directDrawing && QLineF(committed.start, QPointF(100, 60)).length() < .001 &&
        QLineF(committed.end, QPointF(20, 60)).length() < .001, "direct horizontal arrow retains drag direction under zoom and pan");
    const auto arrowImage = ImageDocument::render(source, {committed}, 1).image;
    check(arrowImage != source && arrowImage.pixel(60, 60) != source.pixel(60, 60), "horizontal direct arrow renders without rectangular selection");
    mouse(&canvas, QEvent::MouseButtonPress, canvas.viewportPoint(QPointF(30, 30)));
    check(canvas.cancelDrawing(), "cancel active drawing");
    mouse(&canvas, QEvent::MouseButtonRelease, canvas.viewportPoint(QPointF(80, 80)));
    check(drawn == 1, "cancelled drawing is never committed");
    stroke.variant = 3; stroke.text = "Text"; stroke.strength = 24; canvas.setAnnotation(stroke, true);
    mouse(&canvas, QEvent::MouseButtonPress, canvas.viewportPoint(QPointF(20, 20)));
    mouse(&canvas, QEvent::MouseButtonRelease, canvas.viewportPoint(QPointF(20, 20)));
    check(drawn == 2 && ImageDocument::render(source, {committed}, 1).image != source, "text places with a click");
    canvas.setAnnotation({}, false);
    canvas.hide();

    // Seed only the dedicated test application's settings, avoiding startup registry changes.
    QSettings testSettings(Util::getConfigPath(), QSettings::IniFormat);
    testSettings.setValue("UI_LANGUAGE", "zh"); testSettings.sync();
    SettingModel settings(nullptr);
    settings.setAutoSaveImage(false, temp.path());
    // Exercise the production allocator in ImageBrowserModule.cpp, not just this
    // translation unit: a stale object after a header edit can allocate too little.
    {
        ImageBrowserModule module(&settings);
        module.initialize();
        for (int i = 0; i < 10; ++i) {
            module.open();
            QPointer<ImageBrowserWindow> opened;
            for (auto* widget : QApplication::topLevelWidgets()) {
                if (auto* browser = qobject_cast<ImageBrowserWindow*>(widget)) opened = browser;
            }
            check(opened && opened->isVisible(), "module opens image browser");
            if (!opened) break;
            auto* images = opened->findChild<QListView*>();
            check(waitUntil([&] { return images->isEnabled() && images->model()->rowCount() == 6; }), "module startup completes directory load");
            drop(opened, {QUrl::fromLocalFile(path)});
            check(waitUntil([&] { return action(opened, QStringLiteral("裁剪"))->isEnabled(); }), "module startup loads image");
            opened->close();
            check(waitUntil([&] { return opened.isNull(); }), "module closes and releases browser");
        }
        module.shutdown();
        // Destruction must also cancel a pending directory/image load safely.
        module.open();
        module.shutdown();
        QCoreApplication::processEvents();
    }
    if (qEnvironmentVariableIsSet("NTSCREENSHOT_IMAGE_BROWSER_STARTUP_ONLY")) {
        qInfo() << "Image browser module startup test failures:" << failures;
        return failures ? 1 : 0;
    }
    QPointer<ImageBrowserWindow> window = new ImageBrowserWindow(&settings, &loader);
    check(!action(window, QStringLiteral("设为桌面背景"))->isEnabled() && !action(window, QStringLiteral("设为锁屏背景"))->isEnabled(), "background actions disabled without an image");
    window->show();
    auto* list = window->findChild<QListView*>();
    check(waitUntil([&] { return list->isEnabled() && list->model()->rowCount() == 6; }), "default screenshot directory with autosave disabled");
    drop(window, {QUrl::fromLocalFile(path), QUrl::fromLocalFile(jpeg)});
    check(waitUntil([&] { auto* a = action(window, QStringLiteral("向右旋转")); return a && a->isEnabled(); }), "drop selects image");
    check(window->windowTitle().startsWith(QFileInfo(path).fileName()), "dropped file selected");
    auto* moreMenu = action(window, QStringLiteral("更多"))->menu();
    check(moreMenu && moreMenu->actions().contains(action(window, QStringLiteral("设为桌面背景"))) &&
        moreMenu->actions().contains(action(window, QStringLiteral("设为锁屏背景"))), "background actions available in top-right more menu");
    check(action(window, QStringLiteral("设为桌面背景"))->isEnabled() == SystemBackground::supported() &&
        action(window, QStringLiteral("设为锁屏背景"))->isEnabled() == SystemBackground::supported(), "background actions enabled for supported static image");
    trigger(window, QStringLiteral("编辑"));
    auto* editorPanel = window->findChild<QWidget*>(QStringLiteral("browserEditorPanel"));
    auto* browserCanvas = window->findChild<ImageCanvas*>();
    check(editorPanel && editorPanel->isVisible() && !browserCanvas->cropping(), "edit opens floating panel without entering crop");
    int editorTools = 0;
    for (auto* button : editorPanel->findChildren<QToolButton*>()) if (button->property("editorTool").isValid()) ++editorTools;
    check(editorTools == 9, "nine editor tools");
    for (int tool = 0; tool < 9; ++tool) {
        editorPanel->findChild<QToolButton*>(QStringLiteral("browserEditorTool%1").arg(tool))->click();
        check(!QApplication::activeModalWidget() && !QApplication::activePopupWidget() && editorPanel->isVisible(), "tool details stay inline without popup");
        check(editorPanel->findChild<QStackedWidget*>()->currentIndex() == tool, "lower details match selected tool");
    }
    editorPanel->findChild<QToolButton*>(QStringLiteral("browserEditorTool4"))->click();
    editorPanel->findChild<QPushButton*>(QStringLiteral("editorSubmit"))->click();
    check(!browserCanvas->cropping() && !window->windowTitle().contains('*'), "empty watermark validates inline without editing");
    check(editorPanel->findChild<QLabel*>(QStringLiteral("editorMessage"))->isVisible(), "validation appears inside panel");
    auto* watermarkMode = editorPanel->findChild<QComboBox*>(QStringLiteral("editorWatermarkMode"));
    check(watermarkMode->currentIndex() == 1 && watermarkMode->count() == 7, "watermark defaults to bottom-right preset with seven layouts");
    editorPanel->findChild<QLineEdit*>(QStringLiteral("editorWatermarkText"))->setText("Sample");
    editorPanel->findChild<QPushButton*>(QStringLiteral("editorSubmit"))->click();
    check(waitUntil([&] { return action(window, QStringLiteral("撤销"))->isEnabled(); }), "watermark preset applies without selecting region");
    check(!browserCanvas->cropping(), "watermark preset never enters selection mode");
    trigger(window, QStringLiteral("复制图片"));
    EditOperation uiWatermark; uiWatermark.kind = EditOperation::Watermark; uiWatermark.variant = 1;
    uiWatermark.text = "Sample"; uiWatermark.color = Qt::red; uiWatermark.strength = 36; uiWatermark.opacity = 45;
    check(QApplication::clipboard()->image() == ImageDocument::render(source, {uiWatermark}, 1).image, "preset watermark controls change actual image");
    trigger(window, QStringLiteral("撤销"));
    check(waitUntil([&] { return action(window, QStringLiteral("重做"))->isEnabled(); }), "undo preset watermark");
    watermarkMode->setCurrentIndex(0);
    editorPanel->findChild<QPushButton*>(QStringLiteral("editorSubmit"))->click();
    check(browserCanvas->cropping(), "custom watermark retains region selection");
    watermarkMode->setCurrentIndex(6);
    check(!browserCanvas->cropping(), "switching watermark layout cancels stale selection");
    editorPanel->findChild<QToolButton*>(QStringLiteral("browserEditorTool0"))->click();
    auto* inlineWidth = editorPanel->findChild<QSpinBox*>(QStringLiteral("editorResizeWidth"));
    auto* inlineHeight = editorPanel->findChild<QSpinBox*>(QStringLiteral("editorResizeHeight"));
    inlineWidth->setValue(160);
    check(inlineHeight->value() == 90, "inline resize locks aspect ratio");
    editorPanel->findChild<QToolButton*>(QStringLiteral("browserEditorTool6"))->click();
    check(!window->windowTitle().contains('*'), "switching tools does not commit a draft");
    editorPanel->findChild<QPushButton*>(QStringLiteral("editorSubmit"))->click();
    check(waitUntil([&] { return action(window, QStringLiteral("撤销"))->isEnabled(); }), "filter enters undo history");
    trigger(window, QStringLiteral("复制图片"));
    check(QApplication::clipboard()->image() == gray, "filter UI changes actual document");
    trigger(window, QStringLiteral("撤销"));
    check(waitUntil([&] { return action(window, QStringLiteral("重做"))->isEnabled(); }), "undo filter");
    trigger(window, QStringLiteral("复制图片"));
    check(QApplication::clipboard()->image() == source, "undo restores unedited image");
    editorPanel->findChild<QComboBox*>(QStringLiteral("editorFilter"))->setCurrentIndex(3);
    editorPanel->findChild<QSpinBox*>(QStringLiteral("editorFilterStrength"))->setValue(3);
    editorPanel->findChild<QPushButton*>(QStringLiteral("editorSubmit"))->click();
    check(waitUntil([&] { return action(window, QStringLiteral("撤销"))->isEnabled(); }), "OpenCV blur enters undo history");
    trigger(window, QStringLiteral("复制图片"));
    check(QApplication::clipboard()->image() == ImageDocument::render(source, {blur}, 1).image, "inline OpenCV filter applies selected parameters");
    trigger(window, QStringLiteral("撤销"));
    check(waitUntil([&] { return action(window, QStringLiteral("重做"))->isEnabled(); }), "undo OpenCV filter");
    editorPanel->findChild<QToolButton*>(QStringLiteral("browserEditorTool7"))->click();
    SurfaceStabilityProbe colorSurfaces(window);
    editorPanel->findChild<QSpinBox*>(QStringLiteral("editorTemperature"))->setValue(50);
    editorPanel->findChild<QSpinBox*>(QStringLiteral("editorOpacity"))->setValue(50);
    check(!editorPanel->findChild<QPushButton*>(QStringLiteral("editorSubmit"))->isVisible(), "color changes need no apply button");
    check(editorPanel->findChild<QSpinBox*>(QStringLiteral("editorTemperature"))->isEnabled(), "color fields remain interactive during processing");
    check(action(window, QStringLiteral("编辑"))->isEnabled() && action(window, QStringLiteral("格式转换"))->isEnabled() &&
        action(window, QStringLiteral("复制图片"))->isEnabled(), "color processing keeps unrelated buttons visually available");
    check(waitUntil([&] { return action(window, QStringLiteral("撤销"))->isEnabled(); }), "new color controls enter history");
    trigger(window, QStringLiteral("复制图片"));
    EditOperation uiColor; uiColor.kind = EditOperation::AdjustColor; uiColor.temperature = 50; uiColor.opacity = 50;
    check(QApplication::clipboard()->image() == ImageDocument::render(source, {uiColor}, 1).image, "inline color controls change actual pixels");
    const QString liveColorQa = qEnvironmentVariable("NTSCREENSHOT_LIVE_COLOR_QA_DIR");
    if (!liveColorQa.isEmpty()) {
        QDir().mkpath(liveColorQa);
        window->grab().save(QDir(liveColorQa).filePath("live-color-editor.png"));
    }
    for (int value = 10; value <= 80; ++value) {
        editorPanel->findChild<QSpinBox*>(QStringLiteral("editorTemperature"))->setValue(value);
        QCoreApplication::processEvents();
    }
    action(window, QStringLiteral("复制图片"))->trigger();
    check(waitUntil([&] { return action(window, QStringLiteral("复制图片"))->isEnabled(); }), "latest color values finish automatically");
    uiColor.temperature = 80;
    check(QApplication::clipboard()->image() == ImageDocument::render(source, {uiColor}, 1).image, "color changes use original session pixels without cumulative adjustment");
    check(colorSurfaces.stable(), "live color never restyles, reicons, disables, hides or moves unrelated buttons");
    trigger(window, QStringLiteral("撤销"));
    check(waitUntil([&] { return action(window, QStringLiteral("重做"))->isEnabled(); }), "one undo restores whole color session");
    trigger(window, QStringLiteral("复制图片"));
    check(QApplication::clipboard()->image() == source, "color slider updates are grouped into one undo step");
    trigger(window, QStringLiteral("重做"));
    check(waitUntil([&] { return action(window, QStringLiteral("撤销"))->isEnabled(); }), "redo restores live color result");
    trigger(window, QStringLiteral("复制图片"));
    check(QApplication::clipboard()->image() == ImageDocument::render(source, {uiColor}, 1).image, "redo keeps latest color parameters");
    trigger(window, QStringLiteral("撤销"));
    check(waitUntil([&] { return action(window, QStringLiteral("重做"))->isEnabled(); }), "undo color before testing reset");
    editorPanel->findChild<QSpinBox*>(QStringLiteral("editorTemperature"))->setValue(25);
    check(waitUntil([&] { return action(window, QStringLiteral("撤销"))->isEnabled(); }), "start another live color session");
    editorPanel->findChild<QPushButton*>(QStringLiteral("editorColorReset"))->click();
    check(editorPanel->findChild<QSpinBox*>(QStringLiteral("editorTemperature"))->value() == 0 &&
        editorPanel->findChild<QSpinBox*>(QStringLiteral("editorOpacity"))->value() == 100, "color reset includes new controls");
    check(waitUntil([&] { return action(window, QStringLiteral("复制图片"))->isEnabled(); }), "reset parameters updates image automatically");
    trigger(window, QStringLiteral("复制图片"));
    check(QApplication::clipboard()->image() == source && !window->windowTitle().contains('*'), "reset live color returns to unadjusted session image");
    editorPanel->findChild<QToolButton*>(QStringLiteral("browserEditorTool1"))->click();
    check(browserCanvas->annotating() && !browserCanvas->cropping() && !editorPanel->findChild<QPushButton*>(QStringLiteral("editorSubmit"))->isVisible(), "selecting annotation immediately arms direct drawing");
    browserCanvas->setZoom(1.5);
    const QPointF drawingOrigin = browserCanvas->viewportPoint(QPointF(0, 0));
    mouse(browserCanvas, QEvent::MouseButtonPress, browserCanvas->viewportPoint(QPointF(150, 80)));
    mouse(browserCanvas, QEvent::MouseMove, browserCanvas->viewportPoint(QPointF(180, 110)));
    mouse(browserCanvas, QEvent::MouseButtonRelease, browserCanvas->viewportPoint(QPointF(180, 110)));
    check(waitUntil([&] { return action(window, QStringLiteral("撤销"))->isEnabled(); }), "release commits annotation without selection confirmation");
    check(browserCanvas->annotating(), "drawing remains armed for consecutive annotations");
    check(browserCanvas->zoom() == 1.5 && QLineF(browserCanvas->viewportPoint(QPointF(0, 0)), drawingOrigin).length() < .001,
        "committing annotations preserves zoom and pan");
    trigger(window, QStringLiteral("撤销"));
    check(waitUntil([&] { return action(window, QStringLiteral("重做"))->isEnabled(); }), "undo direct annotation");
    trigger(window, QStringLiteral("复制图片"));
    check(QApplication::clipboard()->image() == source, "undo direct annotation restores original pixels");
    browserCanvas->fit(false);
    editorPanel->findChild<QToolButton*>(QStringLiteral("browserEditorTool0"))->click();
    editorPanel->findChild<QPushButton*>(QStringLiteral("editorSubmit"))->click();
    check(browserCanvas->cropping() && editorPanel->isVisible(), "crop keeps details visible while selecting");
    check(!editorPanel->findChild<QPushButton*>(QStringLiteral("editorApplySelection"))->isEnabled(), "empty selection cannot be applied");
    editorPanel->findChild<QToolButton*>(QStringLiteral("browserEditorTool6"))->click();
    check(!browserCanvas->cropping() && !window->windowTitle().contains('*'), "switching tools cancels uncommitted selection");
    editorPanel->findChild<QToolButton*>(QStringLiteral("browserEditorTool3"))->click();
    check(browserCanvas->annotating() && !browserCanvas->cropping() &&
        !editorPanel->findChild<QPushButton*>(QStringLiteral("editorSubmit"))->isVisible(), "selecting mosaic immediately arms brush without select-area button");
    auto* pixelStyle = editorPanel->findChild<QToolButton*>(QStringLiteral("editorMosaicStyle0"));
    auto* blurStyle = editorPanel->findChild<QToolButton*>(QStringLiteral("editorMosaicStyle1"));
    auto* solidStyle = editorPanel->findChild<QToolButton*>(QStringLiteral("editorMosaicStyle2"));
    check(pixelStyle && blurStyle && solidStyle && pixelStyle->isChecked() &&
        !pixelStyle->icon().isNull() && !blurStyle->icon().isNull() && !solidStyle->icon().isNull(),
        "mosaic offers three visible preview buttons with default selection");
    check(pixelStyle->icon().pixmap(80, 48).toImage() != blurStyle->icon().pixmap(80, 48).toImage() &&
        blurStyle->icon().pixmap(80, 48).toImage() != solidStyle->icon().pixmap(80, 48).toImage(), "style buttons show different actual effects");
    blurStyle->click();
    check(blurStyle->isChecked() && !pixelStyle->isChecked() && qobject_cast<ImageEditorPanel*>(editorPanel)->currentOperation().variant == 1,
        "one click selects blur and updates brush directly");
    solidStyle->click();
    auto* coverColor = editorPanel->findChild<QLineEdit*>(QStringLiteral("editorMosaicColor"));
    check(solidStyle->isChecked() && !blurStyle->isChecked() && coverColor->isVisible() &&
        !editorPanel->findChild<QSpinBox*>(QStringLiteral("editorMosaicBlock"))->isVisible(), "solid cover shows only relevant settings");
    const QImage oldCoverPreview = solidStyle->icon().pixmap(80, 48).toImage();
    coverColor->setText("#ff0000");
    check(oldCoverPreview != solidStyle->icon().pixmap(80, 48).toImage(), "cover thumbnail follows chosen color");
    pixelStyle->click();
    SurfaceStabilityProbe mosaicSurfaces(window);
    editorPanel->findChild<QSpinBox*>(QStringLiteral("editorMosaicBlock"))->setValue(20);
    check(browserCanvas->viewportPoint(QPointF(0, 0)).x() >= browserCanvas->mapFrom(window, editorPanel->geometry().topRight()).x(), "image stays clear of floating panel");
    mouse(browserCanvas, QEvent::MouseButtonPress, browserCanvas->viewportPoint(QPointF(150, 80)));
    mouse(browserCanvas, QEvent::MouseMove, browserCanvas->viewportPoint(QPointF(180, 110)));
    mouse(browserCanvas, QEvent::MouseButtonRelease, browserCanvas->viewportPoint(QPointF(180, 110)));
    check(!editorPanel->findChild<QPushButton*>(QStringLiteral("editorApplySelection"))->isVisible(), "brush needs no selection confirmation");
    check(waitUntil([&] { return action(window, QStringLiteral("撤销"))->isEnabled(); }), "mosaic enters history");
    trigger(window, QStringLiteral("复制图片"));
    EditOperation expectedMosaic; expectedMosaic.kind = EditOperation::Mosaic; expectedMosaic.strength = 20;
    expectedMosaic.directDrawing = true; expectedMosaic.brushPoints = {QPointF(150, 80), QPointF(180, 110)};
    expectedMosaic.brushDiameter = 24/browserCanvas->zoom();
    check(QApplication::clipboard()->image() == ImageDocument::render(source, {expectedMosaic}, 1).image, "mosaic drag commits the circular brush path");
    check(mosaicSurfaces.stable(), "mosaic commit preserves unrelated button styles, icons, enabled states and geometry");
    const QString mosaicQa = qEnvironmentVariable("NTSCREENSHOT_MOSAIC_QA_DIR");
    if (!mosaicQa.isEmpty()) {
        QDir().mkpath(mosaicQa); window->grab().save(QDir(mosaicQa).filePath("mosaic-editor.png"));
    }
    trigger(window, QStringLiteral("撤销"));
    check(waitUntil([&] { return action(window, QStringLiteral("重做"))->isEnabled(); }), "undo mosaic");
    trigger(window, QStringLiteral("复制图片"));
    check(QApplication::clipboard()->image() == source, "one undo restores whole brush stroke");
    mouse(browserCanvas, QEvent::MouseButtonPress, browserCanvas->viewportPoint(QPointF(150, 80)));
    mouse(browserCanvas, QEvent::MouseMove, browserCanvas->viewportPoint(QPointF(180, 110)));
    check(browserCanvas->cancelDrawing(), "active mosaic brush can be cancelled");
    mouse(browserCanvas, QEvent::MouseButtonRelease, browserCanvas->viewportPoint(QPointF(180, 110)));
    check(!window->windowTitle().contains('*'), "cancelled brush never commits pixels");
    drop(window, {QUrl::fromLocalFile(cutoutPath)});
    check(waitUntil([&] { return window->windowTitle().startsWith("cutout-source.png") && action(window, QStringLiteral("编辑"))->isEnabled(); }), "cutout subject loaded");
    editorPanel->findChild<QToolButton*>(QStringLiteral("browserEditorTool8"))->click();
    editorPanel->findChild<QSpinBox*>(QStringLiteral("editorCutoutIterations"))->setValue(2);
    editorPanel->findChild<QSpinBox*>(QStringLiteral("editorCutoutFeather"))->setValue(0);
    editorPanel->findChild<QPushButton*>(QStringLiteral("editorSubmit"))->click();
    check(browserCanvas->cropping() && !QApplication::activeModalWidget(), "cutout selects subject inline");
    mouse(browserCanvas, QEvent::MouseButtonPress, browserCanvas->viewportPoint(QPointF(12, 8)));
    mouse(browserCanvas, QEvent::MouseMove, browserCanvas->viewportPoint(QPointF(84, 88)));
    mouse(browserCanvas, QEvent::MouseButtonRelease, browserCanvas->viewportPoint(QPointF(84, 88)));
    editorPanel->findChild<QPushButton*>(QStringLiteral("editorApplySelection"))->click();
    check(waitUntil([&] { return action(window, QStringLiteral("撤销"))->isEnabled(); }), "cutout UI commits transparent result");
    const QString cvQa = qEnvironmentVariable("NTSCREENSHOT_OPENCV_QA_DIR");
    if (!cvQa.isEmpty()) {
        QDir().mkpath(cvQa); QCoreApplication::processEvents();
        window->grab().save(QDir(cvQa).filePath("cutout-editor.png"));
        cutoutSource.save(QDir(cvQa).filePath("cutout-source.png")); removed.image.save(QDir(cvQa).filePath("cutout-result.png"));
    }
    trigger(window, QStringLiteral("复制图片"));
    check(QApplication::clipboard()->image() == removed.image, "cutout UI matches selected subject and parameters");
    trigger(window, QStringLiteral("撤销"));
    check(waitUntil([&] { return action(window, QStringLiteral("重做"))->isEnabled(); }), "undo cutout");
    trigger(window, QStringLiteral("复制图片")); check(QApplication::clipboard()->image() == cutoutSource, "undo cutout restores full background");
    drop(window, {QUrl::fromLocalFile(path)});
    check(waitUntil([&] { return window->windowTitle().startsWith(QFileInfo(path).fileName()) && action(window, QStringLiteral("编辑"))->isEnabled(); }), "return from cutout to original file");
    trigger(window, QStringLiteral("编辑"));
    check(!editorPanel->isVisible(), "edit toggles floating panel closed");
    auto* details = window->findChild<QLabel*>(QStringLiteral("browserImageDetails"));
    check(details && !details->toolTip().contains(QFileInfo(path).fileName()) && !details->toolTip().contains(temp.path()) &&
          details->toolTip().contains(QLocale().formattedDataSize(QFileInfo(path).size())), "status shows file size without filename or directory");
    trigger(window, QStringLiteral("全屏"));
    check(window->isFullScreen() && action(window, QStringLiteral("全屏"))->isChecked(), "enter fullscreen");
    trigger(window, QStringLiteral("全屏"));
    check(!window->isFullScreen(), "leave fullscreen");
    window->showMaximized();
    check(waitUntil([&] { return window->isMaximized(); }), "maximize before fullscreen");
    QCoreApplication::processEvents();
    trigger(window, QStringLiteral("全屏"));
    QAction* escapeFullscreen = nullptr;
    for (auto* a : window->actions()) if (a->shortcut() == QKeySequence(Qt::Key_Escape)) escapeFullscreen = a;
    check(escapeFullscreen && escapeFullscreen->isEnabled(), "Escape enabled in fullscreen");
    trigger(window, QStringLiteral("裁剪"));
    if (escapeFullscreen) escapeFullscreen->trigger();
    check(window->isFullScreen() && !action(window, QStringLiteral("裁剪"))->isChecked(), "Escape cancels crop before exiting fullscreen");
    if (escapeFullscreen) escapeFullscreen->trigger();
    check(waitUntil([&] { return window->isMaximized(); }), "fullscreen restores maximized state");
    window->showNormal();
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
            check(formats && formats->findData(QByteArray("png")) >= 0 && formats->findData(QByteArray("bmp")) >= 0 && formats->findData(QByteArray("pdf")) >= 0 && formats->findData(QByteArray("ico")) >= 0,
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
    const QString uiPdf = directory.filePath("nested/ui-edited.pdf");
    const QString titleBeforePdf = window->windowTitle();
    QTimer pdfSteps;
    pdfSteps.setInterval(10);
    QObject::connect(&pdfSteps, &QTimer::timeout, &app, [&] {
        auto* modal = QApplication::activeModalWidget();
        if (auto* fileDialog = qobject_cast<QFileDialog*>(modal)) {
            check(fileDialog->defaultSuffix() == "pdf" && fileDialog->nameFilters().first().contains("*.pdf"), "PDF save dialog suffix and filter");
            fileDialog->selectFile(uiPdf); QMetaObject::invokeMethod(fileDialog, "accept", Qt::QueuedConnection);
        } else if (auto* dialog = qobject_cast<ImageConversionDialog*>(modal)) {
            auto* formats = dialog->findChild<QComboBox*>("conversionFormat");
            formats->setCurrentIndex(formats->findData(QByteArray("pdf"))); dialog->accept();
        }
    });
    pdfSteps.start(); trigger(window, QStringLiteral("格式转换")); pdfSteps.stop();
    check(contents(uiPdf).startsWith("%PDF-") && window->windowTitle() == titleBeforePdf &&
        action(window, QStringLiteral("撤销"))->isEnabled(), "PDF conversion exports edited image without losing source or undo history");
    if (!conversionQa.isEmpty()) QFile::copy(uiPdf, QDir(conversionQa).filePath("ui-edited.pdf"));
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
    const QString uiIcon = directory.filePath("nested/ui-icon.ico");
    QTimer icoSteps; icoSteps.setInterval(10);
    QObject::connect(&icoSteps, &QTimer::timeout, &app, [&] {
        auto* modal = QApplication::activeModalWidget();
        if (auto* fileDialog = qobject_cast<QFileDialog*>(modal)) {
            check(fileDialog->defaultSuffix() == "ico", "ICO save dialog uses ico extension");
            fileDialog->selectFile(uiIcon); QMetaObject::invokeMethod(fileDialog, "accept", Qt::QueuedConnection);
        } else if (auto* dialog = qobject_cast<ImageConversionDialog*>(modal)) {
            auto* formats = dialog->findChild<QComboBox*>("conversionFormat");
            formats->setCurrentIndex(formats->findData(QByteArray("ico")));
            auto* sizes = dialog->findChild<QComboBox*>("conversionIcoSize");
            sizes->setCurrentIndex(sizes->findData(32)); dialog->accept();
        }
    });
    icoSteps.start(); trigger(window, QStringLiteral("格式转换")); icoSteps.stop();
    check(waitUntil([&] { return action(window, QStringLiteral("格式转换"))->isEnabled(); }), "ICO conversion reload completes");
    check(QImage(uiIcon).size() == QSize(32, 32) && window->windowTitle().startsWith("ui-icon.ico"), "ICO UI writes and reloads selected icon size");
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
    check(!action(window, QStringLiteral("设为桌面背景"))->isEnabled() && !action(window, QStringLiteral("设为锁屏背景"))->isEnabled(), "animation requires freezing a frame before setting background");
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
                        if (button->isVisible()) {
                            if (!bar->rect().contains(button->geometry())) qWarning() << "Toolbar geometry:" << bar->rect() << button->objectName() << button->geometry();
                            check(bar->rect().contains(button->geometry()), "toolbar button fits at DPI");
                        }
                    }
                    auto* images = qa->findChild<QListView*>();
                    check(images->width() <= qRound(148 * scale), "thumbnail sidebar remains compact");
                    check(images->sizeHintForRow(0) >= images->fontMetrics().height() + qRound(80 * scale), "thumbnail text fits at DPI");
                    check(!qa->findChild<QLabel*>(QStringLiteral("browserDirectoryPath")), "directory removed from status bar");
                    check(bar->styleSheet().contains("border: 0") && bar->styleSheet().contains("spacing: 0"), "borderless compact toolbar");
                    qa->grab().save(QDir(screenshots).filePath(QStringLiteral("browser-%1-%2-%3.png")
                        .arg(theme == AppTheme::Dark ? "dark" : "light").arg(size.width()).arg(qRound(scale * 100))));
                    trigger(qa, QStringLiteral("编辑"));
                    QCoreApplication::processEvents();
                    auto* editor = qa->findChild<QWidget*>(QStringLiteral("browserEditorPanel"));
                    check(editor && qa->rect().contains(editor->geometry()), "editor panel fits at DPI");
                    qa->grab().save(QDir(screenshots).filePath(QStringLiteral("editor-%1-%2-%3.png")
                        .arg(theme == AppTheme::Dark ? "dark" : "light").arg(size.width()).arg(qRound(scale * 100))));
                    if ((size.width() == 1100 && scale == 1) || (size.width() == 560 && scale == 2)) {
                        for (int tool = 0; tool < 9; ++tool) {
                            editor->findChild<QToolButton*>(QStringLiteral("browserEditorTool%1").arg(tool))->click();
                            QCoreApplication::processEvents();
                            qa->grab().save(QDir(screenshots).filePath(QStringLiteral("inline-%1-%2-%3-tool%4.png")
                                .arg(theme == AppTheme::Dark ? "dark" : "light").arg(size.width()).arg(qRound(scale*100)).arg(tool)));
                        }
                        editor->findChild<QToolButton*>(QStringLiteral("browserEditorTool0"))->click();
                        if (size.width() == 560) {
                            auto* scroll = editor->findChild<QScrollArea*>();
                            auto* resizeApply = editor->findChild<QPushButton*>(QStringLiteral("editorResizeApply"));
                            scroll->ensureWidgetVisible(resizeApply);
                            QCoreApplication::processEvents();
                            check(!resizeApply->visibleRegion().isEmpty(), "small-window detail controls reachable by scrolling");
                        }
                    }
                    trigger(qa, QStringLiteral("编辑"));
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
            QImage huge(9000, 8001, QImage::Format_RGB32); huge.fill(Qt::white);
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
