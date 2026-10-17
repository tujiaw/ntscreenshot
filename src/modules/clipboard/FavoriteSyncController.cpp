#include "FavoriteSyncController.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <windows.h>
#include <wincred.h>
#include <limits>

#ifndef NT_CLIPBOARD_SYNC_URL
#define NT_CLIPBOARD_SYNC_URL "https://ntscreenshot.vercel.app"
#endif

namespace {
QString T(const char* source) { return QCoreApplication::translate("App", source); }
constexpr qint64 kMaxImageBytes = 2 * 1024 * 1024;
bool WriteFile(const QString& path, const QByteArray& bytes) {
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}
QByteArray ReadFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    if (file.size() > 10 * 1024 * 1024) return {};
    return file.readAll();
}
}

FavoriteSyncController::FavoriteSyncController(ClipboardHistory& history, ClipboardStore& store, QObject* parent,
                                               QNetworkAccessManager* network, const QString& serviceUrl)
    : QObject(parent), history_(history), store_(store), network_(network ? network : new QNetworkAccessManager(this)) {
    configuredEndpoint_ = serviceUrl.isEmpty() ? QString::fromUtf8(NT_CLIPBOARD_SYNC_URL) : serviceUrl;
    if (configuredEndpoint_.isEmpty()) configuredEndpoint_ = QStringLiteral("https://ntscreenshot.vercel.app");
    retryTimer_.setSingleShot(true);
    debounceTimer_.setSingleShot(true);
    debounceTimer_.setInterval(1500);
    authTimer_.setSingleShot(true);
    connect(&retryTimer_, &QTimer::timeout, this, &FavoriteSyncController::SyncNow);
    connect(&debounceTimer_, &QTimer::timeout, this, &FavoriteSyncController::SyncNow);
    connect(&authTimer_, &QTimer::timeout, this, &FavoriteSyncController::PollSignIn);
}

void FavoriteSyncController::Initialize() {
    endpoint_ = configuredEndpoint_;
    const auto saved = QJsonDocument::fromJson(store_.LoadSettingText("FavoriteSyncState", {}).toUtf8()).object();
    const QString previousEndpoint = saved["endpoint"].toString(store_.LoadSettingText("FavoriteSyncEndpoint", {}));
    // A server change must never reuse another service's session or sync cursor.
    // Local favorites remain available and are merged after the next sign-in.
    const bool sameService = previousEndpoint == endpoint_;
    userId_ = sameService ? saved["userId"].toString() : QString();
    login_ = sameService ? saved["login"].toString() : QString();
    state_.Load(sameService ? saved["state"].toObject() : QJsonObject());
    lastSync_ = QDateTime::fromMSecsSinceEpoch(sameService ? saved["lastSync"].toInteger() : 0);
    if (!userId_.isEmpty()) token_ = ReadCredential();
    SetStatus(SignedIn() ? T("准备同步收藏") : T("登录 GitHub，在设备间同步收藏"));
    if (!userId_.isEmpty()) FavoritesChanged();
    if (SignedIn()) retryTimer_.start(2000);
}

QString FavoriteSyncController::CredentialName() const {
    return QStringLiteral("ntscreenshot/FavoriteSync/") + FavoriteSyncState::Hash(endpoint_.toUtf8()) + '/' + userId_;
}

bool FavoriteSyncController::SaveCredential(const QString& token) {
    const std::wstring target = CredentialName().toStdWString();
    if (token.isEmpty()) {
        return CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0) || GetLastError() == ERROR_NOT_FOUND;
    }
    const QByteArray bytes = token.toUtf8();
    CREDENTIALW credential{};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<wchar_t*>(target.c_str());
    credential.CredentialBlobSize = static_cast<DWORD>(bytes.size());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(bytes.constData()));
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    return CredWriteW(&credential, 0);
}

QString FavoriteSyncController::ReadCredential() const {
    const std::wstring target = CredentialName().toStdWString();
    PCREDENTIALW credential = nullptr;
    if (!CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &credential)) return {};
    const QString token = QString::fromUtf8(reinterpret_cast<const char*>(credential->CredentialBlob), credential->CredentialBlobSize);
    CredFree(credential);
    return QRegularExpression(QStringLiteral("^[a-f0-9]{64}$")).match(token).hasMatch() ? token : QString();
}

bool FavoriteSyncController::Persist() {
    const QJsonObject saved{{"endpoint", endpoint_}, {"userId", userId_}, {"login", login_}, {"state", state_.Save()},
                            {"lastSync", lastSync_.isValid() ? lastSync_.toMSecsSinceEpoch() : 0}};
    return store_.SaveSettingText("FavoriteSyncState", QString::fromUtf8(QJsonDocument(saved).toJson(QJsonDocument::Compact)));
}

QString FavoriteSyncController::CachePath(const QString& hash) const {
    if (!QRegularExpression(QStringLiteral("^[a-f0-9]{64}$")).match(hash).hasMatch()) return {};
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/favorite-sync-images");
    if (!QDir().mkpath(directory)) return {};
    return directory + '/' + hash + QStringLiteral(".png");
}

bool FavoriteSyncController::CacheLocalImages() {
    for (const auto& item : history_.Items()) {
        if (!item.favorite || item.kind != ClipKind::Image) continue;
        if (item.data.size() > kMaxImageBytes) continue; // Local-only oversized favorites do not block sync.
        const QString hash = FavoriteSyncState::Hash(item.data);
        const QString path = CachePath(hash);
        if (path.isEmpty()) return false;
        if (FavoriteSyncState::Hash(ReadFile(path)) != hash && !WriteFile(path, item.data)) return false;
    }
    return true;
}

void FavoriteSyncController::FavoritesChanged() {
    if (applying_ || userId_.isEmpty()) return;
    if (!state_.Reconcile(history_.Items())) return;
    if (!CacheLocalImages() || !Persist()) {
        SetStatus(T("无法保存同步记录，请检查磁盘空间后重试"));
        return;
    }
    emit statusChanged();
    if (SignedIn()) debounceTimer_.start();
}

void FavoriteSyncController::SetStatus(const QString& status) {
    status_ = status;
    emit statusChanged();
}

void FavoriteSyncController::Request(const QString& path, const QByteArray& method, const QByteArray& body,
                                     Reply callback, bool authorized, const QByteArray& contentType) {
    QNetworkRequest request{QUrl(endpoint_ + path)};
    request.setTransferTimeout(30000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setRawHeader("Accept", "application/json");
    request.setHeader(QNetworkRequest::ContentTypeHeader, QString::fromLatin1(contentType));
    if (authorized) request.setRawHeader("Authorization", "Bearer " + token_.toUtf8());
    auto* reply = network_->sendCustomRequest(request, method, body);
    const int generation = generation_;
    connect(reply, &QNetworkReply::readyRead, reply, [reply]() {
        if (reply->bytesAvailable() > 12 * 1024 * 1024) reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, callback = std::move(callback)]() {
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool transportError = reply->error() != QNetworkReply::NoError && httpStatus < 400;
        const QByteArray data = reply->readAll();
        reply->deleteLater();
        if (generation != generation_) return;
        callback(transportError ? 0 : httpStatus, data);
    });
}

void FavoriteSyncController::SignIn() {
    if (SignedIn() || signingIn_) return;
    // Validate stored/build-time URLs too, before sending any credentials.
    const QUrl url(endpoint_);
    if (url.scheme() != QStringLiteral("https") || url.host().isEmpty() || !url.userInfo().isEmpty()) {
        SetStatus(T("请输入有效的 HTTPS 服务地址")); return;
    }
    ++generation_;
    signingIn_ = true;
    SetStatus(T("正在连接 GitHub…"));
    Request("/v1/auth/start", "POST", "{}", [this](int status, const QByteArray& data) {
        if (status != 200) { Fail(status, data); return; }
        const auto result = QJsonDocument::fromJson(data).object();
        ticket_ = result["ticket"].toString();
        userCode_ = result["userCode"].toString();
        verificationUrl_ = result["verificationUrl"].toString();
        if (ticket_.isEmpty() || userCode_.isEmpty() || verificationUrl_ != QStringLiteral("https://github.com/login/device")) {
            Fail(502, {}); return;
        }
        authExpires_ = QDateTime::currentDateTimeUtc().addSecs(qBound(1, result["expiresIn"].toInt(), 900));
        authTimer_.start(qBound(5, result["interval"].toInt(5), 60) * 1000);
        SetStatus(T("在 GitHub 输入验证码，完成后会自动开始同步"));
    }, false);
}

void FavoriteSyncController::PollSignIn() {
    if (!signingIn_) return;
    if (QDateTime::currentDateTimeUtc() >= authExpires_) { Fail(410, "{\"error\":\"authorization_expired\"}"); return; }
    Request("/v1/auth/poll", "POST", QJsonDocument(QJsonObject{{"ticket", ticket_}}).toJson(QJsonDocument::Compact),
        [this](int status, const QByteArray& data) {
            if (status != 200) { Fail(status, data); return; }
            const auto result = QJsonDocument::fromJson(data).object();
            if (result["pending"].toBool()) {
                authTimer_.start(qBound(5, result["interval"].toInt(5), 60) * 1000);
                return;
            }
            const QString token = result["token"].toString();
            const QString user = result["userId"].toString();
            if (!QRegularExpression(QStringLiteral("^[a-f0-9]{64}$")).match(token).hasMatch() || user.isEmpty() || result["login"].toString().isEmpty()) {
                Fail(502, {}); return;
            }
            signingIn_ = false;
            userCode_.clear();
            ticket_.clear();
            token_ = token;
            if (!userId_.isEmpty() && userId_ != user) {
                Request("/v1/auth/logout", "POST", "{}", [](int, const QByteArray&) {});
                token_.clear();
                SetStatus(T("本机收藏已关联 @%1，请登录原账号以避免混合数据").arg(login_));
                return;
            }
            userId_ = user;
            login_ = result["login"].toString();
            if (!SaveCredential(token_)) {
                Request("/v1/auth/logout", "POST", "{}", [](int, const QByteArray&) {});
                token_.clear();
                SetStatus(T("无法安全保存登录凭据，请重试"));
                return;
            }
            FavoritesChanged();
            if (!Persist()) { Fail(507, {}); return; }
            SyncNow();
        }, false);
}

void FavoriteSyncController::CancelSignIn() {
    if (!signingIn_) return;
    ++generation_;
    signingIn_ = false;
    authTimer_.stop();
    ticket_.clear();
    userCode_.clear();
    SetStatus(T("已取消登录，收藏仍保存在本机"));
}

void FavoriteSyncController::SignOut() {
    if (signingIn_) { CancelSignIn(); return; }
    if (!SaveCredential({})) { SetStatus(T("无法移除登录凭据，请重试")); return; }
    ++generation_;
    if (SignedIn()) Request("/v1/auth/logout", "POST", "{}", [](int, const QByteArray&) {});
    token_.clear();
    syncing_ = false;
    retryTimer_.stop();
    debounceTimer_.stop();
    SetStatus(T("已退出登录，本机收藏已保留"));
}

void FavoriteSyncController::SyncNow() {
    if (!SignedIn() || syncing_ || signingIn_) return;
    retryTimer_.stop();
    debounceTimer_.stop();
    if (userId_.isEmpty()) { SignOut(); return; }
    FavoritesChanged();
    if (!CacheLocalImages() || !Persist()) { Fail(507, {}); return; }
    syncing_ = true;
    SetStatus(T("正在同步收藏…"));
    UploadNext(PendingChanges(), 0);
}

QJsonArray FavoriteSyncController::PendingChanges() {
    QJsonArray changes;
    oversizedCount_ = 0;
    qsizetype payloadBytes = 0;
    QMap<QString, bool> imageSizes;
    for (const auto& item : history_.Items()) {
        if (item.favorite && item.kind == ClipKind::Image) {
            imageSizes[FavoriteSyncState::Record(item)["id"].toString()] = item.data.size() > kMaxImageBytes ||
                static_cast<qint64>(item.width) * item.height > 72000000;
        }
    }
    for (const auto& value : state_.Pending((std::numeric_limits<int>::max)())) {
        const auto record = value.toObject();
        bool oversized = false;
        if (!record["deleted"].toBool()) {
            if (record["kind"].toString() == QStringLiteral("text")) oversized = record["text"].toString().toUtf8().size() > 100 * 1024;
            else {
                oversized = imageSizes.value(record["id"].toString(), false);
            }
        }
        if (oversized) { ++oversizedCount_; continue; }
        const auto recordBytes = QJsonDocument(record).toJson(QJsonDocument::Compact).size();
        if (changes.size() < 8 && payloadBytes + recordBytes < 1800 * 1024) {
            changes.append(record);
            payloadBytes += recordBytes;
        }
    }
    return changes;
}

void FavoriteSyncController::UploadNext(QJsonArray changes, int index) {
    if (index >= changes.size()) { Exchange(changes); return; }
    const auto record = changes[index].toObject();
    if (record["deleted"].toBool() || record["kind"].toString() != QStringLiteral("image")) {
        UploadNext(changes, index + 1); return;
    }
    const QString hash = record["imageHash"].toString();
    const QByteArray bytes = ReadFile(CachePath(hash));
    if (bytes.isEmpty() || FavoriteSyncState::Hash(bytes) != hash) { Fail(507, {}); return; }
    if (bytes.size() > kMaxImageBytes) { Fail(413, {}); return; }
    Request("/v1/images/" + hash, "PUT", bytes, [this, changes, index](int status, const QByteArray& data) {
        if (status != 200) { Fail(status, data); return; }
        UploadNext(changes, index + 1);
    }, true, "image/png");
}

void FavoriteSyncController::Exchange(QJsonArray changes) {
    const QJsonObject payload{{"cursor", state_.cursor}, {"changes", changes}};
    Request("/v1/sync", "POST", QJsonDocument(payload).toJson(QJsonDocument::Compact),
        [this](int status, const QByteArray& data) {
            if (status != 200) { Fail(status, data); return; }
            const auto result = QJsonDocument::fromJson(data).object();
            if (!result["changes"].isArray() || !result["acknowledgements"].isArray() ||
                !result["cursor"].isDouble() || result["cursor"].toInteger() < state_.cursor) { Fail(502, {}); return; }
            QStringList hashes;
            // Include local state: a prior interrupted download must be repaired.
            QJsonArray records = result["changes"].toArray();
            for (const auto& value : state_.Favorites()) if (!value.toObject()["dirty"].toBool()) records.append(value);
            for (const auto& value : records) {
                const auto record = value.toObject();
                if (record["deleted"].toBool() || record["kind"].toString() != QStringLiteral("image")) continue;
                const QString hash = record["imageHash"].toString();
                if (CachePath(hash).isEmpty()) { Fail(502, {}); return; }
                if (FavoriteSyncState::Hash(ReadFile(CachePath(hash))) != hash && !hashes.contains(hash)) hashes.append(hash);
            }
            DownloadNext(hashes, 0, result);
        });
}

void FavoriteSyncController::DownloadNext(QStringList hashes, int index, QJsonObject response) {
    if (index >= hashes.size()) { FinishExchange(response); return; }
    const QString hash = hashes[index];
    Request("/v1/images/" + hash, "GET", {}, [this, hashes, index, response, hash](int status, const QByteArray& data) {
        if (status != 200) { Fail(status, data); return; }
        if (data.size() > kMaxImageBytes || FavoriteSyncState::Hash(data) != hash) { Fail(502, {}); return; }
        if (!WriteFile(CachePath(hash), data)) { Fail(507, {}); return; }
        DownloadNext(hashes, index + 1, response);
    });
}

bool FavoriteSyncController::ApplyFavorites() {
    std::vector<ClipItem> items;
    QMap<QString, const ClipItem*> existingFavorites;
    for (const auto& item : history_.Items()) if (item.favorite) existingFavorites[FavoriteSyncState::Record(item)["id"].toString()] = &item;
    for (const auto& value : state_.Favorites()) {
        const auto record = value.toObject();
        const auto* existing = existingFavorites.value(record["id"].toString(), nullptr);
        if (existing) {
            ClipItem kept = *existing;
            kept.capturedAt = QDateTime::fromMSecsSinceEpoch(record["capturedAt"].toInteger());
            items.push_back(std::move(kept));
            continue;
        }
        ClipItem item;
        item.favorite = true;
        item.capturedAt = QDateTime::fromMSecsSinceEpoch(record["capturedAt"].toInteger());
        if (record["kind"].toString() == QStringLiteral("text")) {
            item.text = record["text"].toString();
            if (item.text.isEmpty()) return false;
        } else {
            item.kind = ClipKind::Image;
            item.data = ReadFile(CachePath(record["imageHash"].toString()));
            const QImage image = QImage::fromData(item.data, "PNG");
            if (image.isNull()) return false;
            item.width = image.width();
            item.height = image.height();
        }
        items.push_back(std::move(item));
    }
    std::stable_sort(items.begin(), items.end(), [](const ClipItem& a, const ClipItem& b) { return a.capturedAt > b.capturedAt; });
    for (const auto& item : history_.Items()) if (!item.favorite) items.push_back(item);
    const auto& previous = history_.Items();
    const bool unchanged = items.size() == previous.size() && std::equal(items.begin(), items.end(), previous.begin(),
        [](const ClipItem& a, const ClipItem& b) {
            return a.favorite == b.favorite && a.kind == b.kind && a.capturedAt == b.capturedAt &&
                a.text == b.text && a.data == b.data && a.width == b.width && a.height == b.height;
        });
    if (unchanged) return true; // Background checks must not reset the selected row.
    if (!store_.Save(items, history_.MaxItems())) return false;
    applying_ = true;
    history_.ReplaceItems(std::move(items));
    emit favoritesUpdated();
    applying_ = false;
    return true;
}

void FavoriteSyncController::FinishExchange(const QJsonObject& response) {
    const auto before = state_.Save();
    state_.Acknowledge(response["acknowledgements"].toArray());
    state_.Merge(response["changes"].toArray());
    state_.cursor = response["cursor"].toInteger();
    if (!ApplyFavorites()) { state_.Load(before); Fail(507, {}); return; }
    if (!Persist()) { Fail(507, {}); return; }
    const auto pending = PendingChanges();
    if (response["more"].toBool() || !pending.isEmpty()) {
        UploadNext(pending, 0);
        return;
    }
    syncing_ = false;
    retrySeconds_ = 30;
    lastSync_ = QDateTime::currentDateTimeUtc();
    if (!Persist()) { Fail(507, {}); return; }
    SetStatus(oversizedCount_ > 0 ? T("其他收藏已同步，%1 项超出大小限制，保留在本机").arg(oversizedCount_)
                                : T("收藏已同步"));
    retryTimer_.start(5 * 60 * 1000);
}

void FavoriteSyncController::Fail(int status, const QByteArray& response) {
    syncing_ = false;
    const bool wasSigningIn = signingIn_;
    signingIn_ = false;
    authTimer_.stop();
    userCode_.clear();
    ticket_.clear();
    const QString code = QJsonDocument::fromJson(response).object()["error"].toString();
    QString message;
    if (status == 401) {
        SaveCredential({});
        token_.clear();
        message = T("登录已过期，请重新登录 GitHub");
    } else if (code == QStringLiteral("account_not_allowed")) message = T("此 GitHub 账号尚未获准使用该同步服务");
    else if (code == QStringLiteral("service_not_configured")) message = T("云同步服务尚未配置 GitHub 登录，请联系服务提供者");
    else if (code == QStringLiteral("access_denied")) message = T("已取消 GitHub 授权，可以重新登录");
    else if (code == QStringLiteral("authorization_expired") || status == 410) message = T("验证码已过期，请重新登录");
    else if (code == QStringLiteral("text_too_large")) message = T("单条收藏文字超过 100 KB，请缩短后重试");
    else if (code == QStringLiteral("storage_quota_exceeded")) message = T("云端图片空间已满，请联系服务提供者");
    else if (code == QStringLiteral("sync_quota_exceeded")) message = T("收藏同步空间已满，请稍后重试或联系支持");
    else if (status == 413) message = T("收藏图片超过 2 MB 或尺寸过大，请缩小后重试");
    else if (status == 507) message = T("无法保存同步记录，请检查磁盘空间后重试");
    else if (status == 429) message = T("请求过于频繁，稍后重试");
    else if (wasSigningIn) message = T("暂时无法登录，请检查网络后重试");
    else message = T("暂时无法同步，本机收藏已保留，将自动重试");
    SetStatus(message);
    if (SignedIn() && !wasSigningIn) {
        retryTimer_.start(retrySeconds_ * 1000);
        retrySeconds_ = qMin(retrySeconds_ * 2, 5 * 60);
    }
}
