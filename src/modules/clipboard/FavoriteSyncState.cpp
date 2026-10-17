#include "FavoriteSyncState.h"
#include <QCryptographicHash>
#include <QSet>
#include <QUuid>

QString FavoriteSyncState::Hash(const QByteArray& bytes) {
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

QJsonObject FavoriteSyncState::Record(const ClipItem& item) {
    QJsonObject record{{"deleted", false}, {"capturedAt", item.capturedAt.toMSecsSinceEpoch()}};
    if (item.kind == ClipKind::Text) {
        record["kind"] = "text";
        record["text"] = item.text;
        record["id"] = Hash("text:" + item.text.toUtf8());
    } else {
        const QString imageHash = Hash(item.data);
        record["kind"] = "image";
        record["imageHash"] = imageHash;
        record["width"] = item.width;
        record["height"] = item.height;
        record["id"] = Hash("image:" + imageHash.toLatin1());
    }
    return record;
}

void FavoriteSyncState::Load(const QJsonObject& json) {
    entries_.clear();
    cursor = json["cursor"].toInteger();
    for (const auto& value : json["entries"].toArray()) {
        const auto entry = value.toObject();
        if (!entry["id"].toString().isEmpty()) entries_[entry["id"].toString()] = entry;
    }
}

QJsonObject FavoriteSyncState::Save() const {
    QJsonArray entries;
    for (const auto& entry : entries_) entries.append(entry);
    return {{"cursor", cursor}, {"entries", entries}};
}

bool FavoriteSyncState::Reconcile(const std::vector<ClipItem>& items) {
    bool changed = false;
    QSet<QString> present;
    for (const auto& item : items) {
        if (!item.favorite) continue;
        auto record = Record(item);
        const QString id = record["id"].toString();
        present.insert(id);
        if (entries_.contains(id) && !entries_[id]["deleted"].toBool() &&
            entries_[id]["capturedAt"] == record["capturedAt"]) continue;
        record["revision"] = entries_.value(id)["revision"].toInteger();
        record["dirty"] = true;
        record["operationId"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
        entries_[id] = record;
        changed = true;
    }
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
        if (present.contains(it.key()) || (*it)["deleted"].toBool()) continue;
        (*it)["deleted"] = true;
        (*it)["dirty"] = true;
        (*it)["operationId"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
        // Tombstones contain no clipboard payload.
        (*it).remove("text");
        (*it).remove("imageHash");
        changed = true;
    }
    return changed;
}

QJsonArray FavoriteSyncState::Pending(int limit) const {
    QJsonArray pending;
    for (auto entry : entries_) {
        if (!entry["dirty"].toBool()) continue;
        entry.remove("dirty");
        entry.remove("revision");
        pending.append(entry);
        if (pending.size() >= limit) break;
    }
    return pending;
}

void FavoriteSyncState::Acknowledge(const QJsonArray& acknowledgements) {
    for (const auto& value : acknowledgements) {
        const auto ack = value.toObject();
        auto it = entries_.find(ack["id"].toString());
        // An edit made while the request was in flight must stay dirty.
        if (it == entries_.end() || (*it)["operationId"] != ack["operationId"]) continue;
        (*it)["dirty"] = false;
        (*it)["revision"] = ack["revision"];
    }
}

void FavoriteSyncState::Merge(const QJsonArray& changes) {
    for (const auto& value : changes) {
        auto incoming = value.toObject();
        const QString id = incoming["id"].toString();
        const auto local = entries_.value(id);
        if (id.isEmpty() || local["dirty"].toBool() ||
            incoming["revision"].toInteger() <= local["revision"].toInteger()) continue;
        incoming["dirty"] = false;
        entries_[id] = incoming;
    }
}

QJsonArray FavoriteSyncState::Favorites() const {
    QJsonArray favorites;
    for (const auto& entry : entries_) if (!entry["deleted"].toBool()) favorites.append(entry);
    return favorites;
}

int FavoriteSyncState::PendingCount() const {
    int count = 0;
    for (const auto& entry : entries_) if (entry["dirty"].toBool()) ++count;
    return count;
}
