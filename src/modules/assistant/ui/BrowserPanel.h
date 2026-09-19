#pragma once
#include <QWidget>
#include <QJsonObject>
#include <QElapsedTimer>
#include <memory>
#include <QMutex>
#include <QList>

class QWebEngineView;
class QWebEngineProfile;
class QLineEdit;
class QPushButton;
class QTimer;
class QLabel;
namespace LlmTools { class ToolAbort; }

// One off-the-record session per conversation. All WebEngine access stays on the GUI thread.
class BrowserPanel final : public QWidget {
    Q_OBJECT
public:
    explicit BrowserPanel(QWidget *parent = nullptr, int authenticationWaitMs = 60000);
    ~BrowserPanel() override;
    QString execute(const QJsonObject &args, LlmTools::ToolAbort *abort);
    void cancel();
    void clearSnapshotCache();
    void takeOver(const QString &reason);
    void resume();
    void resetAuthenticationWait();
signals:
    void attentionRequired(const QString &reason);
    void activityRequested();
    void collapseRequested();
private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void continueWithoutLogin();
    void updateAuthenticationCountdown();
    struct Request;
    void start(const QJsonObject &args, const std::shared_ptr<Request> &request);
    void observe();
    // 页面离开认证环节后自动把控制权交还 AI（取代原来的“已完成，继续”按钮）。
    void checkAutoResume();
    void finish(const QString &result, bool notifyUser = false);
    void navigate(const QString &url);
    // 回到内置搜索起始页。
    void loadHome();
    // 把当前主题注入起始页：WebEngine 的 prefers-color-scheme 跟系统，不跟应用主题。
    void installHomeThemeScript();
    QWebEngineView *view_;
    QWebEngineProfile *profile_;
    QLineEdit *address_;
    QTimer *timer_;
    QWidget *authenticationBar_ = nullptr;
    QLabel *authenticationCountdown_ = nullptr;
    int authenticationWaitMs_;
    bool authenticationWaitSkipped_ = false;
    std::shared_ptr<Request> request_;
    QMutex queuedRequestsMutex_;
    QList<std::shared_ptr<Request>> queuedRequests_;
    bool manual_ = false;
    bool loading_ = false;
    bool evaluating_ = false;
    quint64 generation_ = 0;
    QElapsedTimer settled_;
    int settleMs_ = 0;
    // 用户在网页操作后重置；无操作到期则继续未登录任务。
    QElapsedTimer manualWait_;
};
