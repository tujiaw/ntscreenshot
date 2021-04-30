#pragma once

#include <QObject>
#include <QApplication>
#include <QAbstractNativeEventFilter>

class Application : public QApplication, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    Application(int argc, char **argv);
    ~Application();

protected:
    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result);
};
