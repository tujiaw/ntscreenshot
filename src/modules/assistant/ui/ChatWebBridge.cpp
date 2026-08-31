#include "modules/assistant/ui/ChatWebBridge.h"

ChatWebBridge::ChatWebBridge(QObject *parent)
    : QObject(parent)
{
}

void ChatWebBridge::notifyReady()
{
    emit sigReady();
}

void ChatWebBridge::requestRetry()
{
    emit sigRetryRequested();
}
