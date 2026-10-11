#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shobjidl.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.System.UserProfile.h>
#endif

#include "SystemBackground.h"
#include "core/imaging/ImageLimits.h"
#include <QBuffer>
#include <QCoreApplication>
#include <QColorSpace>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QPainter>
#include <QSaveFile>
#include <QStandardPaths>

namespace SystemBackground {
namespace {
QString message(const char* source) { return QCoreApplication::translate("SystemBackground", source); }
#ifdef Q_OS_WIN
struct Apartment {
    Apartment() { winrt::init_apartment(winrt::apartment_type::multi_threaded); }
    ~Apartment() { winrt::uninit_apartment(); }
};
winrt::com_ptr<IDesktopWallpaper> desktopWallpaper() {
    winrt::com_ptr<IDesktopWallpaper> wallpaper;
    // This class is provided by a local COM service on Windows. Requesting
    // only an in-process DLL fails with REGDB_E_CLASSNOTREG (0x80040154).
    winrt::check_hresult(CoCreateInstance(CLSID_DesktopWallpaper, nullptr,
        CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
        __uuidof(IDesktopWallpaper), wallpaper.put_void()));
    return wallpaper;
}
QString systemError(const winrt::hresult_error& error) {
    return message("系统未能设置背景：%1（错误码 %2）。")
        .arg(QString::fromWCharArray(error.message().c_str()))
        .arg(QString::number(quint32(error.code().value), 16));
}
#endif
}

bool supported() {
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

Result checkAvailability(Target target) {
    Result result;
#ifdef Q_OS_WIN
    try {
        Apartment apartment;
        if (target == Target::Desktop) {
            UINT count = 0;
            winrt::check_hresult(desktopWallpaper()->GetMonitorDevicePathCount(&count));
        } else {
            winrt::get_activation_factory<winrt::Windows::Storage::StorageFile,
                winrt::Windows::Storage::IStorageFileStatics>();
            winrt::get_activation_factory<winrt::Windows::System::UserProfile::LockScreen,
                winrt::Windows::System::UserProfile::ILockScreenStatics>();
        }
        result.success = true;
    } catch (const winrt::hresult_error& error) {
        result.error = systemError(error);
    } catch (const std::exception&) {
        result.error = message("背景图片处理失败。");
    }
#else
    Q_UNUSED(target);
    result.error = message("当前系统不支持设置背景。");
#endif
    return result;
}

Result prepare(const QImage& image, const QString& cacheDirectory) {
    Result result;
    if (image.isNull() || qint64(image.width()) * image.height() > ImageLimits::MaxEditablePixels) {
        result.error = message("没有可用的背景图片。"); return result;
    }
    QImage opaque(image.size(), QImage::Format_RGB32);
    if (opaque.isNull()) { result.error = message("背景图片处理失败。"); return result; }
    opaque.fill(Qt::white);
    opaque.setColorSpace(image.colorSpace());
    { QPainter painter(&opaque); painter.drawImage(0, 0, image); }
    QByteArray encoded;
    QBuffer buffer(&encoded);
    if (!buffer.open(QIODevice::WriteOnly) || !opaque.save(&buffer, "JPEG", 95)) {
        result.error = message("背景图片处理失败。"); return result;
    }
    if (cacheDirectory.isEmpty() || !QDir().mkpath(cacheDirectory)) {
        result.error = message("无法创建背景图片缓存目录。"); return result;
    }
    const QString name = QString::fromLatin1(QCryptographicHash::hash(encoded, QCryptographicHash::Sha256).toHex())
        + QStringLiteral(".jpg");
    result.path = QDir(cacheDirectory).absoluteFilePath(name);
    // Keep the snapshot after the browser closes or the original file is moved/deleted.
    QSaveFile file(result.path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly) || file.write(encoded) != encoded.size() || !file.commit()) {
        result.error = file.errorString(); return result;
    }
    result.success = true;
    return result;
}

Result apply(const QImage& image, Target target) {
    Result result;
    if (!supported()) { result.error = message("当前系统不支持设置背景。"); return result; }
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (root.isEmpty()) { result.error = message("无法创建背景图片缓存目录。"); return result; }
    result = prepare(image, QDir(root).filePath(QStringLiteral("backgrounds")));
    if (!result.success) return result;
    result.success = false;
#ifdef Q_OS_WIN
    try {
        Apartment apartment;
        const std::wstring path = QDir::toNativeSeparators(result.path).toStdWString();
        if (target == Target::Desktop) {
            const auto wallpaper = desktopWallpaper();
            // NULL monitor ID applies the current image to all monitors without changing the fit mode.
            winrt::check_hresult(wallpaper->SetWallpaper(nullptr, path.c_str()));
        } else {
            const auto file = winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(winrt::hstring(path)).get();
            winrt::Windows::System::UserProfile::LockScreen::SetImageFileAsync(file).get();
        }
        result.success = true;
    } catch (const winrt::hresult_error& error) {
        result.error = systemError(error);
    } catch (const std::exception&) {
        result.error = message("背景图片处理失败。");
    }
#else
    Q_UNUSED(target);
#endif
    return result;
}
}
