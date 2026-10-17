#pragma once

#include "ClipboardHistory.h"
#include "ClipboardStore.h"
#include "FavoriteSyncState.h"
#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>
#include <functional>

class FavoriteSyncController final : public QObject {
    Q_OBJECT
public:
    FavoriteSyncController(ClipboardHistory& history, ClipboardStore& store, QObject* parent = nullptr,
                           QNetworkAccessManager* network = nullptr, const QString& serviceUrl = {});
    void Initialize();
    void FavoritesChanged();
    void SignIn();
    void CancelSignIn();
    void SignOut();
    void SyncNow();
    QString Endpoint() const { return endpoint_; }
    QString Account() const { return login_; }
    QString Status() const { return status_; }
    QString UserCode() const { return userCode_; }
    QString VerificationUrl() const { return verificationUrl_; }
    bool SignedIn() const { return !token_.isEmpty(); }
    bool SigningIn() const { return signingIn_; }
    bool Busy() const { return syncing_; }
    int PendingCount() const { return state_.PendingCount(); }
    QDateTime LastSync() const { return lastSync_; }
signals:
    void statusChanged();
    void favoritesUpdated();
private:
    using Reply = std::function<void(int, const QByteArray&)>;
    void Request(const QString& path, const QByteArray& method, const QByteArray& body, Reply callback,
                 bool authorized = true, const QByteArray& contentType = "application/json");
    void PollSignIn();
    void UploadNext(QJsonArray changes, int index);
    QJsonArray PendingChanges();
    void Exchange(QJsonArray changes);
    void DownloadNext(QStringList hashes, int index, QJsonObject response);
    void FinishExchange(const QJsonObject& response);
    void Fail(int status, const QByteArray& response);
    void SetStatus(const QString& status);
    bool Persist();
    QString CachePath(const QString& hash) const;
    bool CacheLocalImages();
    bool ApplyFavorites();
    QString CredentialName() const;
    bool SaveCredential(const QString& token);
    QString ReadCredential() const;

    ClipboardHistory& history_;
    ClipboardStore& store_;
    FavoriteSyncState state_;
    QNetworkAccessManager* network_;
    QTimer retryTimer_, debounceTimer_, authTimer_;
    QString endpoint_, configuredEndpoint_, token_, userId_, login_, status_;
    QString ticket_, userCode_, verificationUrl_;
    QDateTime authExpires_, lastSync_;
    bool syncing_ = false, signingIn_ = false, applying_ = false;
    int generation_ = 0, retrySeconds_ = 30;
    int oversizedCount_ = 0;
};
