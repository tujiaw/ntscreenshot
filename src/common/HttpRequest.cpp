#include "HttpRequest.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QHttpMultiPart>
#include <QDebug>
#include <QSsl>

HttpRequest::HttpRequest(QObject *parent)
    : QObject(parent)
    , manager_(new QNetworkAccessManager(this))
{
    connect(manager_, SIGNAL(finished(QNetworkReply*)), this, SLOT(replyFinished(QNetworkReply*)), Qt::QueuedConnection);
}

HttpRequest::~HttpRequest()
{
}

void HttpRequest::get(const QString &url)
{
    QNetworkReply *reply = manager_->get(QNetworkRequest(QUrl(url)));
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(slotError(QNetworkReply::NetworkError)));
}

void HttpRequest::postForm(const QString &url, const QByteArray &data)
{
    qDebug() << "url:" << url << "," << data;
    QUrl aurl(url);
    QNetworkRequest req(aurl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    QNetworkReply *reply = manager_->post(req, data);
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(slotError(QNetworkReply::NetworkError)));
}

void HttpRequest::postImage(const QString &url, const QString& name, const QByteArray &data)
{
    QString suffix = "png";
    if (name.lastIndexOf(".") > 0) {
        suffix = name.mid(name.lastIndexOf(".") + 1);
    }
    QHttpPart part1;
    part1.setHeader(QNetworkRequest::ContentTypeHeader, QString("image/%1").arg(suffix));
    part1.setHeader(QNetworkRequest::ContentDispositionHeader, QString("form-data; name=\"file\"; filename=%1").arg(name));
    part1.setBody(data);

    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    multiPart->append(part1);

    QUrl aurl(url);
    QNetworkRequest req(aurl);

    //QSslConfiguration conf = req.sslConfiguration();
    //conf.setPeerVerifyMode(QSslSocket::VerifyNone);
    //conf.setProtocol(QSsl::TlsV1_0);
    //req.setSslConfiguration(conf);

    QNetworkReply *reply = manager_->post(req, multiPart);
    multiPart->setParent(reply);

    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(slotError(QNetworkReply::NetworkError)));
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
