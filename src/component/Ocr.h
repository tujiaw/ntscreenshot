#pragma once

#include <QObject>

class HttpRequest;
class Ocr : public QObject
{
    Q_OBJECT

public:
    Ocr(QObject* parent);
    ~Ocr();
    void start(const QPixmap& pixmap);

private slots:
    void onHttpResponse(int err, const QByteArray& data);

private:
    HttpRequest* m_http;
};
