#pragma once

#include <QObject>
#include <QByteArray>
#include <QElapsedTimer>

#include "core/settings/SettingModel.h"

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;
class QUrl;

class PaddleOcrClient : public QObject
{
    Q_OBJECT

public:
    explicit PaddleOcrClient(QObject* parent = nullptr);
    void recognize(const QByteArray& pngData, const PaddleOcrConfig& config);

signals:
    void succeeded(const QString& text);
    void failed(const QString& error);

private:
    void pollStatus();
    void fetchResult(const QUrl& url);
    void fail(const QString& error);

    QNetworkAccessManager* manager_ = nullptr;
    QTimer* pollTimer_ = nullptr;
    PaddleOcrConfig config_;
    QString jobId_;
    QElapsedTimer elapsed_;
    bool finished_ = false;
};
