#pragma once

#include <QObject>

class ChatWebBridge : public QObject
{
    Q_OBJECT

public:
    explicit ChatWebBridge(QObject *parent = nullptr);

signals:
    void sigReady();
    void hydrateMessages(const QString &json);
    void appendMessage(const QString &json);
    void updateMessage(const QString &json);
    void removeMessage(const QString &id);
    void clearMessages();
    void requestStateChanged(bool pending);
    void sigRetryRequested();
    /** JSON: { "action":"executing"|"executed"|"clear", "name", "args"|"result" } */
    void toolActivity(const QString &json);

public slots:
    void notifyReady();
    void requestRetry();
};
