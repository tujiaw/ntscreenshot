#include "modules/clipboard/FavoriteSyncState.h"
#include <QCoreApplication>
#include <cstdio>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "Failed line %d: %s\n", __LINE__, #x); return 1; } } while (false)

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    ClipItem local;
    local.text = QStringLiteral("favorite");
    local.favorite = true;
    ClipItem ordinary;
    ordinary.text = QStringLiteral("must stay local");
    FavoriteSyncState state;
    CHECK(state.Reconcile({local, ordinary}));
    CHECK(state.PendingCount() == 1 && state.Favorites().size() == 1);
    const auto addition = state.Pending()[0].toObject();
    CHECK(addition["text"].toString() == local.text);
    CHECK(!state.Reconcile({local, ordinary}));
    CHECK(state.Pending()[0].toObject()["operationId"] == addition["operationId"]);
    FavoriteSyncState restarted;
    restarted.Load(state.Save());
    CHECK(restarted.Pending() == state.Pending());

    // A delete while an upload is in flight must survive its acknowledgement.
    CHECK(state.Reconcile({ordinary}));
    const auto deletion = state.Pending()[0].toObject();
    CHECK(deletion["deleted"].toBool() && !deletion.contains("text"));
    state.Acknowledge({QJsonObject{{"id", addition["id"]}, {"operationId", addition["operationId"]}, {"revision", 1}}});
    CHECK(state.PendingCount() == 1);
    auto remoteAddition = addition;
    remoteAddition["revision"] = 1;
    state.Merge({remoteAddition});
    CHECK(state.Favorites().isEmpty());
    state.Acknowledge({QJsonObject{{"id", deletion["id"]}, {"operationId", deletion["operationId"]}, {"revision", 2}}});
    CHECK(state.PendingCount() == 0);
    state.Merge({remoteAddition});
    CHECK(state.Favorites().isEmpty());

    // Newer cross-device re-adds win after the local operation was accepted.
    remoteAddition["revision"] = 3;
    state.Merge({remoteAddition});
    CHECK(state.Favorites().size() == 1);
    auto remoteDeletion = deletion;
    remoteDeletion["revision"] = 4;
    state.Merge({remoteDeletion});
    CHECK(state.Favorites().isEmpty());
    state.cursor = 4;
    restarted.Load(state.Save());
    CHECK(restarted.cursor == 4 && restarted.Favorites().isEmpty());
    CHECK(restarted.Reconcile({local}));
    CHECK(restarted.PendingCount() == 1 && !restarted.Pending()[0].toObject()["deleted"].toBool());

    // Same image bytes share an ID; text and images occupy separate namespaces.
    ClipItem image;
    image.kind = ClipKind::Image;
    image.favorite = true;
    image.data = QByteArray("png data");
    image.width = image.height = 1;
    const auto imageRecord = FavoriteSyncState::Record(image);
    CHECK(imageRecord["imageHash"].toString() == FavoriteSyncState::Hash(image.data));
    CHECK(imageRecord["id"] != addition["id"]);
    CHECK(restarted.Reconcile({local, image}));
    CHECK(restarted.PendingCount() == 2);
    const auto oldOperation = restarted.Save()["entries"].toArray();
    local.capturedAt = local.capturedAt.addSecs(1);
    CHECK(restarted.Reconcile({local, image}));
    CHECK(restarted.Save()["entries"].toArray() != oldOperation);
    CHECK(restarted.PendingCount() == 2);
    return 0;
}
