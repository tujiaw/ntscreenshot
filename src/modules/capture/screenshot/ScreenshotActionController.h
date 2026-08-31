#pragma once

#include <QObject>
#include <QPixmap>
#include <QString>
#include <QPoint>

class ScreenshotActionController : public QObject {
    Q_OBJECT
public:
    explicit ScreenshotActionController(class WindowManager* windowManager, QObject *parent = nullptr);

signals:
    void sigClose();

public slots:
    void onUploadRequested(const QPixmap &pixmap);
    void onLLMChatRequested(const QString &prompt, const QPixmap &pixmap);
    void onSaveRequested(const QPixmap &pixmap);
    void onSaveToClipboardRequested(const QPixmap &pixmap);
    void onStickerRequested(const QPixmap &pixmap, const QPoint &pos);

private:
    WindowManager* windowManager_ = nullptr;
};
