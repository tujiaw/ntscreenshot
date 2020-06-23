#pragma once

#include <QObject>
#include <QNetworkReply>

class QNetworkAccessManager;
class HttpRequest : public QObject
{
    Q_OBJECT

public:
    HttpRequest(QObject *parent);
    ~HttpRequest();
    void get(const QString &url);
    void postForm(const QString &url, const QByteArray &data);
    void postImage(const QString &url, const QString &name, const QByteArray &data);

signals:
    void sigHttpResponse(int err, const QByteArray &data);

private slots:
    void replyFinished(QNetworkReply*);
    void slotError(QNetworkReply::NetworkError);

private:
    QNetworkAccessManager *manager_;
};
