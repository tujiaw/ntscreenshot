#include "ScreenshotActionController.h"
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QFileDialog>
#include <QTimer>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <thread>

#include "core/platform/Util.h"
#include "shared/ui/TipsWidget.h"
#include "app/WindowManager.h"
#include "core/settings/SettingModel.h"
#include "modules/capture/pin/PinWidget.h"

ScreenshotActionController::ScreenshotActionController(WindowManager* windowManager, QObject *parent)
    : QObject(parent)
    , windowManager_(windowManager) {
}

void ScreenshotActionController::onUploadRequested(const QPixmap &pixmap) {
    if (pixmap.isNull()) {
        qDebug() << "ScreenshotActionController::onUploadRequested pixmap is null";
        return;
    }

    QString imgName = QString("ntscreenshot-%1.png").arg(QDateTime::currentDateTime().toString("hhmmss"));
    auto *setting = windowManager_->setting();
    const GitHubImageBedConfig ghConfig = setting->gitHubImageBedConfig();
    if (ghConfig.token.isEmpty()) {
        return;
    }

    emit sigClose();
    
    QPixmap p = pixmap;
    std::thread([p, imgName, ghConfig]() {
        Util::UploadResult result = Util::uploadToGitHub(Util::pixmap2ByteArray(p), imgName, ghConfig);
        
        QMetaObject::invokeMethod(qApp, [result]() {
            if (result.success) {
                QApplication::clipboard()->setText(result.fullUrl);
            } else {
                QMessageBox::warning(nullptr, QStringLiteral("上传图床失败"), result.message);
            }
        }, Qt::QueuedConnection);
    }).detach();
}

void ScreenshotActionController::onLLMChatRequested(const QString &prompt, const QPixmap &pixmap) {
    if (pixmap.isNull()) return;
    emit sigClose();
    windowManager_->showLlmChatWindow(QStringLiteral("AI 对话"), prompt, pixmap);
}

void ScreenshotActionController::onSaveRequested(const QPixmap &pixmap) {
    if (pixmap.isNull()) return;

    QString name = Util::pixmapName();
    QString filePath = PinWidget::saveDir().absoluteFilePath(name);
    
    emit sigClose();
    
    QPixmap p = pixmap;
    QTimer::singleShot(0, [p, filePath]() {
        QString fileName = QFileDialog::getSaveFileName(nullptr, QStringLiteral("保存图片"), filePath, "PNG (*.png)");
        if (!fileName.isEmpty()) {
            PinWidget::setSaveDir(QFileInfo(fileName).absoluteDir());
            p.save(fileName, "png");
        }
    });
}

void ScreenshotActionController::onSaveToClipboardRequested(const QPixmap &pixmap) {
    if (pixmap.isNull()) return;
    QClipboard *board = QApplication::clipboard();
    board->setPixmap(pixmap);
    emit sigClose();
}

void ScreenshotActionController::onStickerRequested(const QPixmap &pixmap, const QPoint &pos) {
    if (pixmap.isNull()) return;
    QPoint stickerPos = pos;
    if (PinWidget::hasBorder(windowManager_->setting())) {
        stickerPos -= QPoint(1, 1);
    }
    emit sigClose();
    WindowManager* windowManager = windowManager_;
    QTimer::singleShot(0, [windowManager, pixmap, stickerPos]() {
        PinWidget::popup(windowManager, pixmap, stickerPos);
    });
}
