#pragma once

#include "ClipItem.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <vector>

// Content-addressed, immutable favorites. Dirty operations survive restarts and
// retries retain their operation ID. Server revisions order cross-device edits.
class FavoriteSyncState {
public:
    static QString Hash(const QByteArray& bytes);
    static QJsonObject Record(const ClipItem& item);
    void Load(const QJsonObject& json);
    QJsonObject Save() const;
    bool Reconcile(const std::vector<ClipItem>& items);
    QJsonArray Pending(int limit = 8) const;
    void Acknowledge(const QJsonArray& acknowledgements);
    void Merge(const QJsonArray& changes);
    QJsonArray Favorites() const;
    int PendingCount() const;
    qint64 cursor = 0;

private:
    QMap<QString, QJsonObject> entries_;
};
