#include "HttpRequest.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QHttpMultiPart>
#include <QDebug>
#include <QSsl>
#include <QFileInfo>

QString NetworkUrl::chatCompletions(const QString &baseUrl)
{
    QString url = baseUrl.trimmed();
    while (url.endsWith('/')) {
        url.chop(1);
    }

    const QString suffix = QStringLiteral("/chat/completions");
    if (!url.endsWith(suffix, Qt::CaseInsensitive)) {
        url += suffix;
    }
    return url;
}

HttpRequest::HttpRequest(QObject *parent)
    : QObject(parent)
    , manager_(new QNetworkAccessManager(this))
{
    connect(manager_, &QNetworkAccessManager::finished, this, &HttpRequest::replyFinished, Qt::QueuedConnection);
}

HttpRequest::~HttpRequest()
{
}

void HttpRequest::get(const QString &url)
{
    QNetworkReply *reply = manager_->get(QNetworkRequest(QUrl(url)));
    connect(reply, &QNetworkReply::errorOccurred, this, &HttpRequest::slotError);
}

void HttpRequest::postForm(const QString &url, const QByteArray &data)
{
    QUrl aurl(url);
    QNetworkRequest req(aurl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    QNetworkReply *reply = manager_->post(req, data);
    connect(reply, &QNetworkReply::errorOccurred, this, &HttpRequest::slotError);
}

void HttpRequest::postImage(const QString &url, const QString& name, const QByteArray &data)
{
    // Use QFileInfo to get extension
    QFileInfo fileInfo(name);
    QString suffix = fileInfo.suffix();
    if (suffix.isEmpty()) {
        suffix = "png";
    }

    QHttpPart part1;
    part1.setHeader(QNetworkRequest::ContentTypeHeader, QString("image/%1").arg(suffix));
    part1.setHeader(QNetworkRequest::ContentDispositionHeader, QString("form-data; name=\"file\"; filename=\"%1\"").arg(name));
    part1.setBody(data);

    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    multiPart->append(part1);

    QNetworkRequest req{QUrl(url)};
    QNetworkReply *reply = manager_->post(req, multiPart);
    multiPart->setParent(reply);

    connect(reply, &QNetworkReply::errorOccurred, this, &HttpRequest::slotError);
}

void HttpRequest::replyFinished(QNetworkReply *reply)
{
    if (!reply) {
        qDebug() << "replyFinished is null!!!";
        return;
    }

    emit sigHttpResponse(reply->error(), reply->readAll());
    reply->deleteLater();
}

void HttpRequest::slotError(QNetworkReply::NetworkError err)
{
    if (err != QNetworkReply::NoError) {
        qDebug() << "http response error: " << err;
    }
}
