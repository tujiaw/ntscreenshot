#pragma once

#include <QMap>
#include <QList>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <algorithm>

namespace Agent {

struct ToolCall {
    QString id;
    QString name;
    QString arguments;
};

struct ToolCallAccumulator {
    struct PartialToolCall {
        QString id;
        QString name;
        QString arguments;
    };

    QMap<int, PartialToolCall> partials;

    void accumulate(int index, const QString &id, const QString &nameDelta, const QString &argsDelta) {
        auto &partial = partials[index];
        if (!id.isEmpty()) {
            partial.id = id;
        }
        if (!nameDelta.isEmpty()) {
            partial.name += nameDelta;
        }
        if (!argsDelta.isEmpty()) {
            partial.arguments += argsDelta;
        }
    }

    QList<ToolCall> toToolCalls() const {
        QList<ToolCall> result;
        auto keys = partials.keys();
        std::sort(keys.begin(), keys.end());
        for (int idx : keys) {
            const auto &p = partials[idx];
            result.append({p.id, p.name, p.arguments});
        }
        return result;
    }

    QJsonArray toJsonArray() const {
        QJsonArray arr;
        auto calls = toToolCalls();
        for (const auto &call : calls) {
            QJsonObject funcObj;
            funcObj["name"] = call.name;
            funcObj["arguments"] = call.arguments;

            QJsonObject tcObj;
            tcObj["id"] = call.id;
            tcObj["type"] = QStringLiteral("function");
            tcObj["function"] = funcObj;
            arr.append(tcObj);
        }
        return arr;
    }

    void reset() { partials.clear(); }
    bool isEmpty() const { return partials.isEmpty(); }
};

} // namespace Agent
