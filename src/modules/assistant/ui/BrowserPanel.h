#pragma once
#include <QWidget>
#include <QJsonObject>
#include <QElapsedTimer>
#include <memory>

class QWebEngineView;
class QWebEngineProfile;
class QLineEdit;
class QPushButton;
class QTimer;
namespace LlmTools { class ToolAbort; }

// One off-the-record session per conversation. All WebEngine access stays on the GUI thread.
class BrowserPanel final : public QWidget {
    Q_OBJECT
public:
    explicit BrowserPanel(QWidget *parent = nullptr);
    ~BrowserPanel() override;
    QString execute(const QJsonObject &args, LlmTools::ToolAbort *abort);
    void cancel();
    void clearSnapshotCache();
    void takeOver(const QString &reason);
    void resume();
signals:
    void attentionRequired(const QString &reason);
    void activityRequested();
    void collapseRequested();
private:
    struct Request;
    void start(const QJsonObject &args, const std::shared_ptr<Request> &request);
    void observe();
    // 页面离开认证环节后自动把控制权交还 AI（取代原来的“已完成，继续”按钮）。
    void checkAutoResume();
    void finish(const QString &result, bool notifyUser = false);
    void navigate(const QString &url);
    // 回到起始页（介绍 browser use 的静态页面）。
    void loadHome();
    // 把当前主题注入起始页：WebEngine 的 prefers-color-scheme 跟系统，不跟应用主题。
    void installHomeThemeScript();
    QWebEngineView *view_;
    QWebEngineProfile *profile_;
    QLineEdit *address_;
    QTimer *timer_;
    std::shared_ptr<Request> request_;
    bool manual_ = false;
    bool loading_ = false;
    bool evaluating_ = false;
    quint64 generation_ = 0;
    QElapsedTimer settled_;
    int settleMs_ = 0;
    // 本次交出控制权的时间；超过 kManualWaitTimeoutMs 仍未恢复则放弃等待。
    QElapsedTimer manualWait_;
};
