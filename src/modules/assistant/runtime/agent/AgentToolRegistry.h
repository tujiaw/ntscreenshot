#pragma once

#include <QObject>
#include <QJsonArray>
#include <QMap>
#include <QSharedPointer>
#include <QStringList>
#include "modules/assistant/runtime/tools/LlmTool.h"

namespace Agent {

class ToolRegistry : public QObject {
    Q_OBJECT

public:
    explicit ToolRegistry(QObject *parent = nullptr);

    void registerTool(QSharedPointer<LlmTools::LlmTool> tool);
    void unregisterTool(const QString &name);
    bool hasTool(const QString &name) const;
    QStringList toolNames() const;

    QJsonArray definitions() const;
    QString execute(const QString &name, const QString &argumentsJson,
                    LlmTools::ToolAbort *abort = nullptr) const;

signals:
    void sigToolRegistered(const QString &name);
    void sigToolUnregistered(const QString &name);

private:
    QMap<QString, QSharedPointer<LlmTools::LlmTool>> tools_;
};

} // namespace Agent
