#include "modules/clipboard/FavoriteSyncController.h"
#include "modules/clipboard/FavoriteSyncPage.h"
#include "modules/clipboard/HistoryWindow.h"
#include "core/theme/ThemeManager.h"
#include <QApplication>
#include <QBuffer>
#include <QEventLoop>
#include <QImage>
#include <QJsonDocument>
#include <QLabel>
#include <QLayout>
#include <QNetworkReply>
#include <QPushButton>
#include <QRandomGenerator>
#include <QTemporaryDir>
#include <QUuid>
#include <cstring>
#include <cstdio>

#define CHECK_SYNC(x) do { if (!(x)) { std::fprintf(stderr, "Sync test failed line %d: %s\n", __LINE__, #x); return 1; } } while (false)

namespace {
class FakeReply final : public QNetworkReply {
public:
    FakeReply(const QNetworkRequest& request, int status, QByteArray data, QObject* parent)
        : QNetworkReply(parent), data_(std::move(data)) {
        setRequest(request);
        setUrl(request.url());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        QTimer::singleShot(0, this, [this]() { setFinished(true); emit readyRead(); emit finished(); });
    }
    void abort() override {}
    qint64 bytesAvailable() const override { return data_.size() - offset_ + QNetworkReply::bytesAvailable(); }
protected:
    qint64 readData(char* output, qint64 maxSize) override {
        const qint64 count = qMin(maxSize, data_.size() - offset_);
        if (count <= 0) return -1;
        std::memcpy(output, data_.constData() + offset_, static_cast<size_t>(count));
        offset_ += count;
        return count;
    }
private:
    QByteArray data_;
    qint64 offset_ = 0;
};

class FakeSyncService final : public QNetworkAccessManager {
public:
    bool offline = false;
    int imageDownloads = 0;
    QString userId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QJsonArray events;
    QMap<QString, QByteArray> images;

    FakeSyncService() {
        ClipItem text;
        text.text = QStringLiteral("from another device");
        Append(FavoriteSyncState::Record(text));
        ClipItem image;
        image.kind = ClipKind::Image;
        image.width = image.height = 8;
        QImage pixel(8, 8, QImage::Format_ARGB32);
        for (int y = 0; y < 8; ++y) for (int x = 0; x < 8; ++x) {
            pixel.setPixel(x, y, 0xff000000u | QRandomGenerator::global()->generate());
        }
        QBuffer buffer(&image.data);
        buffer.open(QIODevice::WriteOnly);
        pixel.save(&buffer, "PNG");
        const auto record = FavoriteSyncState::Record(image);
        images[record["imageHash"].toString()] = image.data;
        Append(record);
    }
    qint64 Append(QJsonObject record) {
        record["operationId"] = record["operationId"].toString(QUuid::createUuid().toString(QUuid::WithoutBraces));
        record["revision"] = events.size() + 1;
        events.append(record);
        return events.size();
    }
protected:
    QNetworkReply* createRequest(Operation, const QNetworkRequest& request, QIODevice* outgoing) override {
        const QByteArray bytes = outgoing ? outgoing->readAll() : QByteArray();
        const auto input = QJsonDocument::fromJson(bytes).object();
        const QString path = request.url().path();
        QJsonObject result;
        if (offline) return new FakeReply(request, 503, "{}", this);
        if (path == QStringLiteral("/v1/auth/start")) {
            result = {{"ticket", QString(64, 'a')}, {"userCode", "K7PM-4QTX"},
                      {"verificationUrl", "https://github.com/login/device"}, {"expiresIn", 900}, {"interval", 5}};
        } else if (path == QStringLiteral("/v1/auth/poll")) {
            result = {{"token", QString(64, 'e')}, {"userId", userId}, {"login", "test-user"}};
        } else if (path.startsWith(QStringLiteral("/v1/images/"))) {
            const QString hash = path.mid(QStringLiteral("/v1/images/").size());
            if (request.attribute(QNetworkRequest::CustomVerbAttribute).toByteArray() == "PUT") {
                images[hash] = bytes;
                result = {{"ok", true}};
            } else {
                ++imageDownloads;
                return new FakeReply(request, images.contains(hash) ? 200 : 404, images.value(hash), this);
            }
        } else if (path == QStringLiteral("/v1/sync")) {
            QJsonArray acknowledgements;
            for (const auto& value : input["changes"].toArray()) {
                const auto record = value.toObject();
                qint64 revision = 0;
                for (const auto& event : events) if (event.toObject()["operationId"] == record["operationId"]) revision = event.toObject()["revision"].toInteger();
                if (!revision) revision = Append(record);
                acknowledgements.append(QJsonObject{{"id", record["id"]}, {"operationId", record["operationId"]}, {"revision", revision}});
            }
            QJsonArray changes;
            for (const auto& event : events) if (event.toObject()["revision"].toInteger() > input["cursor"].toInteger()) changes.append(event);
            result = {{"acknowledgements", acknowledgements}, {"changes", changes}, {"cursor", events.size()}, {"more", false}};
        } else if (path == QStringLiteral("/v1/auth/logout")) result = {{"ok", true}};
        else return new FakeReply(request, 404, "{}", this);
        return new FakeReply(request, 200, QJsonDocument(result).toJson(QJsonDocument::Compact), this);
    }
};

bool WaitUntil(const std::function<bool()>& condition, int timeoutMs = 8000) {
    QEventLoop loop;
    QTimer tick, deadline;
    tick.setInterval(10);
    deadline.setSingleShot(true);
    QObject::connect(&tick, &QTimer::timeout, &loop, [&]() { if (condition()) loop.quit(); });
    QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    tick.start();
    deadline.start(timeoutMs);
    if (!condition()) loop.exec();
    return condition();
}
}

int runFavoriteSyncControllerTest(QApplication& app, const QString& previewDir) {
    QTemporaryDir directory;
    CHECK_SYNC(directory.isValid());
    const QByteArray previousDirectory = qgetenv("LOCALAPPDATA");
    qputenv("LOCALAPPDATA", directory.path().toLocal8Bit());
    int result = [&]() -> int {
        ClipboardStore store;
        CHECK_SYNC(store.Available());
        ClipboardHistory history;
        history.AddText(QStringLiteral("only local history"));
        ClipItem favorite;
        favorite.favorite = true;
        favorite.text = QStringLiteral("local favorite");
        ClipItem oversized = favorite;
        oversized.text = QString(100 * 1024 + 1, 'x');
        auto items = history.Items();
        items.insert(items.begin(), {favorite, oversized});
        history.ReplaceItems(items);
        CHECK_SYNC(store.Save(history.Items(), history.MaxItems()));
        FakeSyncService network;
        const QString endpoint = QStringLiteral("https://sync-") + network.userId + QStringLiteral(".example.invalid");
        FavoriteSyncController controller(history, store, nullptr, &network, endpoint);
        struct Cleanup { FavoriteSyncController& controller; ~Cleanup() { controller.SignOut(); } } cleanup{controller};
        controller.Initialize();
        HistoryWindow window(1.0);
        auto* page = new FavoriteSyncPage(&controller);
        window.SetSyncPage(page);
        window.ShowSyncPage();
        window.resize(340, 460);
        auto& dialog = *page;
        CHECK_SYNC(!dialog.isWindow());
        if (!previewDir.isEmpty()) {
            ThemeManager::apply(AppTheme::Dark);
            window.show();
            app.processEvents();
        }
        auto* login = dialog.findChild<QPushButton*>(QStringLiteral("favoriteSyncSignIn"));
        CHECK_SYNC(login && login->isEnabled());
        login->click();
        CHECK_SYNC(WaitUntil([&]() { return !controller.UserCode().isEmpty(); }));
        CHECK_SYNC(controller.SigningIn());
        auto* views = window.findChild<QButtonGroup*>(QStringLiteral("clipViewGroup"));
        CHECK_SYNC(views && views->checkedId() == 2);
        views->button(1)->click();
        CHECK_SYNC(controller.SigningIn() && !page->isVisible());
        window.ShowSyncPage();
        CHECK_SYNC(views->checkedId() == 2 && controller.SigningIn());
        auto* code = dialog.findChild<QLabel*>(QStringLiteral("favoriteSyncUserCode"));
        CHECK_SYNC(code && code->text() == QStringLiteral("K7PM-4QTX"));
        if (!previewDir.isEmpty()) {
            app.processEvents();
            CHECK_SYNC(code->geometry().bottom() < code->parentWidget()->height());
            for (auto* button : code->parentWidget()->findChildren<QPushButton*>()) {
                CHECK_SYNC(button->geometry().bottom() < code->parentWidget()->height());
            }
            int previousBottom = -1;
            auto* content = dialog.findChild<QWidget*>(QStringLiteral("favoriteSyncContent"));
            CHECK_SYNC(content && content->layout());
            for (int i = 0; i < content->layout()->count(); ++i) {
                auto* widget = content->layout()->itemAt(i)->widget();
                if (!widget || !widget->isVisible()) continue;
                if (widget->geometry().top() <= previousBottom) {
                    std::fprintf(stderr, "Overlapping sync widget %s: y=%d bottom=%d previous=%d dialogHeight=%d minimum=%d hint=%d\n",
                        widget->metaObject()->className(), widget->geometry().top(), widget->geometry().bottom(), previousBottom,
                        dialog.height(), dialog.minimumHeight(), dialog.sizeHint().height());
                    dialog.grab().save(previewDir + QStringLiteral("/sync-layout-debug.png"));
                }
                CHECK_SYNC(widget->geometry().top() > previousBottom);
                previousBottom = widget->geometry().bottom();
            }
            CHECK_SYNC(window.grab().save(previewDir + QStringLiteral("/sync-authorization.png")));
        }
        CHECK_SYNC(WaitUntil([&]() { return controller.SignedIn() && !controller.Busy(); }));
        CHECK_SYNC(history.Items().size() == 5);
        CHECK_SYNC(controller.PendingCount() == 1); // Oversized text stays local.
        CHECK_SYNC(network.imageDownloads == 1);
        CHECK_SYNC(controller.Status().contains(QStringLiteral("1")));
        CHECK_SYNC(!store.LoadSettingText("FavoriteSyncState", {}).contains(QString(64, 'e')));
        for (const auto& event : network.events) CHECK_SYNC(event.toObject()["text"].toString() != QStringLiteral("only local history"));
        if (!previewDir.isEmpty()) {
            app.processEvents();
            CHECK_SYNC(window.grab().save(previewDir + QStringLiteral("/sync-connected.png")));
        }
        // A local deletion remains pending across a network outage.
        size_t remoteIndex = 0;
        while (remoteIndex < history.Items().size() && history.Items()[remoteIndex].text != QStringLiteral("from another device")) ++remoteIndex;
        CHECK_SYNC(remoteIndex < history.Items().size() && history.RemoveAt(remoteIndex));
        CHECK_SYNC(store.Save(history.Items(), history.MaxItems()));
        controller.FavoritesChanged();
        network.offline = true;
        controller.SyncNow();
        CHECK_SYNC(WaitUntil([&]() { return !controller.Busy(); }));
        CHECK_SYNC(controller.PendingCount() == 2 && history.Items().size() == 4);
        network.offline = false;
        controller.SyncNow();
        CHECK_SYNC(WaitUntil([&]() { return !controller.Busy(); }));
        CHECK_SYNC(controller.PendingCount() == 1 && history.Items().size() == 4);
        CHECK_SYNC(network.events.last().toObject()["deleted"].toBool());
        const auto eventsBefore = network.events.size();
        controller.SyncNow();
        CHECK_SYNC(WaitUntil([&]() { return !controller.Busy(); }));
        CHECK_SYNC(network.events.size() == eventsBefore);
        {
            FavoriteSyncController restarted(history, store, nullptr, &network, endpoint);
            restarted.Initialize();
            CHECK_SYNC(restarted.SignedIn() && restarted.PendingCount() == 1);
        }
        controller.SignOut();
        CHECK_SYNC(!controller.SignedIn() && history.Items().size() == 4);
        return 0;
    }();
    qputenv("LOCALAPPDATA", previousDirectory);
    return result;
}
