#include "Ocr.h"
#include <QDateTime>
#include <QVariantMap>
#include <QUrl>
#include <QCryptographicHash>
#include <QDebug>
#include <QClipboard>
#include <QApplication>
#include "common/Util.h"
#include "common/HttpRequest.h"
#include "component/TipsWidget.h"

const qint64 APPID = 2156348148;
const QString APPKEY = "XFpS522Lm1kPcAQ3";
const QString OCRURL = "https://api.ai.qq.com/fcgi-bin/ocr/ocr_generalocr";

Ocr::Ocr(QObject* parent) : QObject(parent)
{
	m_http = new HttpRequest(this);
	connect(m_http, &HttpRequest::sigHttpResponse, this, &Ocr::onHttpResponse);
}

Ocr::~Ocr()
{

}

void Ocr::onHttpResponse(int err, const QByteArray& data)
{
	QWidget* parentWidget = qobject_cast<QWidget*>(this->parent());
	if (err != 0) {
		QString tips = QString("network error:%1").arg(err);
		TipsWidget::popup(parentWidget, tips, 5);
		return;
	}

	QVariantMap vm = Util::json2map(data);
	if (vm["ret"].toInt() == 0) {
		QVariantList vl = vm["data"].toMap()["item_list"].toList();
		QStringList strList;
		foreach(const QVariant & v, vl) {
			strList << v.toMap()["itemstring"].toString();
		}

		QString text = strList.join("\n");
		QClipboard* clipboard = QApplication::clipboard();
		clipboard->setText(text);
	}
	TipsWidget::popup(parentWidget, vm["msg"].toString(), 5);
}

void Ocr::start(const QPixmap& pixmap)
{
	QWidget* parentWidget = qobject_cast<QWidget*>(this->parent());
	QByteArray b = Util::pixmap2ByteArray(pixmap);
	if (b.size() > 1024 * 1024) {
		TipsWidget::popup(parentWidget, QStringLiteral("图片不能大于1M！"), 5);
		return;
	}

	QList<QPair<QString, QVariant>> params;
	params.push_back(QPair<QString, QVariant>("app_id", APPID));
	params.push_back(QPair<QString, QVariant>("time_stamp", QDateTime::currentSecsSinceEpoch()));
	params.push_back(QPair<QString, QVariant>("nonce_str", "123456"));
	params.push_back(QPair<QString, QVariant>("sign", QVariant()));
	params.push_back(QPair<QString, QVariant>("image", QString(b.toBase64())));
	auto getSign = [&]() -> QString {
		qSort(params.begin(), params.end(), [](auto& left, auto& right) -> bool {
			return left.first < right.first;
			});
		QString s;
		foreach(auto & param, params) {
			if (!param.second.isNull()) {
				QString v = QString(QUrl::toPercentEncoding(param.second.toString()));
				s += QString("%1=%2&").arg(param.first).arg(v);
			}
		}
		s += QString("app_key=%1").arg(APPKEY);
		return QString::fromUtf8(QCryptographicHash::hash(s.toLocal8Bit(), QCryptographicHash::Md5).toHex()).toUpper();
	};

	QString sign = getSign();
	QString paramsStr;
	for (int i = 0; i < params.size(); i++) {
		if (params.at(i).first == "sign") {
			paramsStr += QString("%1=%2&").arg(params.at(i).first).arg(sign);
		}
		else {
			QString v = QString(QUrl::toPercentEncoding(params.at(i).second.toString()));
			paramsStr += QString("%1=%2&").arg(params.at(i).first).arg(v);
		}
	}
	if (!paramsStr.isEmpty() && paramsStr[paramsStr.size() - 1] == '&') {
		paramsStr = paramsStr.mid(0, paramsStr.size() - 1);
	}
	m_http->postForm(OCRURL, paramsStr.toLocal8Bit());
}
