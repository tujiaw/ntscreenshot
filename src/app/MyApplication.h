#pragma once

#include <QObject>
#include <QApplication>
#include <QAbstractNativeEventFilter>

class MyApplication : public QApplication, public QAbstractNativeEventFilter
{
	Q_OBJECT
public:
	MyApplication(int &argc, char **argv);
	~MyApplication();

protected:
	bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
};
