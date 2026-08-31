#include "PaddleOcrClient.h"

#include <QHttpMultiPart>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>

namespace {
constexpr int kRequestTimeoutMs = 30000;
constexpr int kPollTimeoutMs = 180000;
constexpr int kPollIntervalMs = 2000;

QString responseError(QNetworkReply* reply, const QJsonObject& object)
{
    const QString apiMessage = object.value(QStringLiteral("msg")).toString().trimmed();
    if (!apiMessage.isEmpty() && apiMessage.compare(QStringLiteral("success"), Qt::CaseInsensitive) != 0) {
        return apiMessage;
    }
    const QString networkMessage = reply->errorString().trimmed();
    return networkMessage.isEmpty() ? QStringLiteral("OCR 服务请求失败") : networkMessage;
}

void appendTextValues(const QJsonValue& value, QStringList& output)
{
    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        const QJsonValue texts = object.value(QStringLiteral("rec_texts"));
        if (texts.isArray()) {
            for (const QJsonValue& item : texts.toArray()) {
                const QString text = item.toString().trimmed();
                if (!text.isEmpty()) output.push_back(text);
            }
        }
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            if (it.key() != QStringLiteral("rec_texts")) appendTextValues(it.value(), output);
        }
    } else if (value.isArray()) {
        for (const QJsonValue& item : value.toArray()) appendTextValues(item, output);
    }
}

QString parseResultText(const QByteArray& content)
{
    QStringList texts;
    const QList<QByteArray> lines = content.split('\n');
    for (const QByteArray& rawLine : lines) {
        const QByteArray line = rawLine.trimmed();
        if (line.isEmpty()) continue;
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(line, &error);
        if (error.error == QJsonParseError::NoError) {
            appendTextValues(document.isObject() ? QJsonValue(document.object())
                                                  : QJsonValue(document.array()), texts);
        }
    }
    return texts.join(QLatin1Char('\n'));
}

QNetworkRequest authorizedRequest(const QUrl& url, const QString& token)
{
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QByteArray("bearer ") + token.toUtf8());
    request.setTransferTimeout(kRequestTimeoutMs);
    return request;
}
}

PaddleOcrClient::PaddleOcrClient(QObject* parent)
    : QObject(parent)
    , manager_(new QNetworkAccessManager(this))
    , pollTimer_(new QTimer(this))
{
    pollTimer_->setSingleShot(true);
    pollTimer_->setInterval(kPollIntervalMs);
    connect(pollTimer_, &QTimer::timeout, this, &PaddleOcrClient::pollStatus);
}

void PaddleOcrClient::recognize(const QByteArray& pngData, const PaddleOcrConfig& config)
{
    if (pngData.isEmpty()) return fail(QStringLiteral("截图数据为空"));
    if (!config.enabled) return fail(QStringLiteral("OCR 功能未启用"));
    if (!QUrl(config.jobUrl).isValid() || config.token.isEmpty() || config.model.isEmpty()) {
        return fail(QStringLiteral("请先完善 PaddleOCR 配置"));
    }

    config_ = config;
    config_.jobUrl.remove(QRegularExpression(QStringLiteral("/+$")));
    elapsed_.start();

    auto* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    const auto addTextPart = [multiPart](const QByteArray& name, const QByteArray& value) {
        QHttpPart part;
        part.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QStringLiteral("form-data; name=\"%1\"").arg(QString::fromUtf8(name)));
        part.setBody(value);
        multiPart->append(part);
    };
    addTextPart("model", config_.model.toUtf8());
    addTextPart("optionalPayload",
                R"({"useDocOrientationClassify":false,"useDocUnwarping":false,"useTextlineOrientation":false})");

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("image/png"));
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QStringLiteral("form-data; name=\"file\"; filename=\"screenshot.png\""));
    filePart.setBody(pngData);
    multiPart->append(filePart);

    QNetworkReply* reply = manager_->post(
        authorizedRequest(QUrl(config_.jobUrl), config_.token), multiPart);
    multiPart->setParent(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray body = reply->readAll();
        const QJsonDocument document = QJsonDocument::fromJson(body);
        const QJsonObject object = document.object();
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || httpStatus < 200 || httpStatus >= 300
            || object.value(QStringLiteral("code")).toInt(-1) != 0) {
            const QString error = responseError(reply, object);
            reply->deleteLater();
            return fail(error);
        }
        jobId_ = object.value(QStringLiteral("data")).toObject()
                     .value(QStringLiteral("jobId")).toString().trimmed();
        reply->deleteLater();
        if (jobId_.isEmpty()) return fail(QStringLiteral("OCR 服务未返回任务编号"));
        pollTimer_->start();
    });
}

void PaddleOcrClient::pollStatus()
{
    if (finished_) return;
    if (elapsed_.elapsed() >= kPollTimeoutMs) return fail(QStringLiteral("OCR 识别超时，请稍后重试"));

    const QUrl url(config_.jobUrl + QLatin1Char('/') + jobId_);
    QNetworkReply* reply = manager_->get(authorizedRequest(url, config_.token));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray body = reply->readAll();
        const QJsonObject object = QJsonDocument::fromJson(body).object();
        if (reply->error() != QNetworkReply::NoError
            || object.value(QStringLiteral("code")).toInt(-1) != 0) {
            const QString error = responseError(reply, object);
            reply->deleteLater();
            return fail(error);
        }

        const QJsonObject data = object.value(QStringLiteral("data")).toObject();
        const QString state = data.value(QStringLiteral("state")).toString().toLower();
        reply->deleteLater();
        if (state == QStringLiteral("done")) {
            const QString resultUrl = data.value(QStringLiteral("resultUrl")).toObject()
                                          .value(QStringLiteral("jsonUrl")).toString();
            if (resultUrl.isEmpty()) return fail(QStringLiteral("OCR 结果地址为空"));
            return fetchResult(QUrl(resultUrl));
        }
        if (state == QStringLiteral("failed")) {
            const QString error = data.value(QStringLiteral("errorMsg")).toString();
            return fail(error.isEmpty() ? QStringLiteral("OCR 识别失败") : error);
        }
        if (state != QStringLiteral("pending") && state != QStringLiteral("running")) {
            return fail(QStringLiteral("OCR 返回了未知任务状态"));
        }
        pollTimer_->start();
    });
}

void PaddleOcrClient::fetchResult(const QUrl& url)
{
    QNetworkRequest request(url);
    request.setTransferTimeout(kRequestTimeoutMs);
    QNetworkReply* reply = manager_->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            const QString error = reply->errorString();
            reply->deleteLater();
            return fail(error);
        }
        const QString text = parseResultText(body);
        reply->deleteLater();
        if (text.isEmpty()) return fail(QStringLiteral("未识别到文字"));
        finished_ = true;
        emit succeeded(text);
    });
}

void PaddleOcrClient::fail(const QString& error)
{
    if (finished_) return;
    finished_ = true;
    pollTimer_->stop();
    emit failed(error.isEmpty() ? QStringLiteral("OCR 识别失败") : error);
}
