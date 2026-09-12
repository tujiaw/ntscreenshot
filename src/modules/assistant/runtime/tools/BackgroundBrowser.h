#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

namespace LlmTools {
class ToolAbort;

// Owned by the conversation on the GUI thread; tools run on the agent thread.
class BackgroundBrowser : public QObject {
public:
    using QObject::QObject;
    QString read(const QUrl &url, int maxChars, ToolAbort *abort, int timeoutMs = 25000);
    QString readArticle(const QUrl &url, ToolAbort *abort);
    QString evaluate(const QUrl &url, const QString &script, ToolAbort *abort);
private:
    QString run(const QUrl &url, const QString &script, int maxResultChars, ToolAbort *abort, int timeoutMs = 25000);
};
}
