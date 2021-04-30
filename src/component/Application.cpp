#include "Application.h"
#include <Windows.h>
#include <QDebug>

Application::Application(int argc, char **argv)
    : QApplication(argc, argv)
{
}

Application::~Application()
{
}

bool Application::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
{
    //if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
    //}
    return false;
}