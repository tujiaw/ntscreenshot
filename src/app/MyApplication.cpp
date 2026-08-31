#include "MyApplication.h"

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif
#include <QDebug>

MyApplication::MyApplication(int &argc, char **argv)
	: QApplication(argc, argv)
{
}

MyApplication::~MyApplication()
{
}

bool MyApplication::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
	Q_UNUSED(eventType);
	Q_UNUSED(message);
	Q_UNUSED(result);
	return false;
}
