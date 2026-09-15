#pragma once

#include <QString>
#include <QLabel>
#include <QPushButton>
#include <QPoint>
#include <QPixmap>
#include <QList>
#include <QSize>
#include <QVariantList>
#include "shared/ui/FramelessWidget.h"

namespace Agent {
class AgentRunner;
class ToolRegistry;
}
class QWebEngineView;
class ChatWebBridge;
class ChatInputWidget;
class BrowserPanel;
class QResizeEvent;
class QShowEvent;
class QSplitter;
class QVBoxLayout;
class QWidget;
class SettingModel;
namespace LlmTools { class BackgroundBrowser; }

class ChatWidget : public FramelessWidget
{
    Q_OBJECT

public:
    explicit ChatWidget(
        SettingModel* settings,
        const QString &title,
        const QString &message,
        const QSize &windowSize,
        QWidget *parent = nullptr);
    ~ChatWidget();

    void startChatWithText(const QString &text);
    void startChatWithImage(const QString &text, const QPixmap &image);
    void quoteTextInInput(const QString &text);
    void quoteImageInInput(const QPixmap &image, const QString &displayName = QStringLiteral("image.png"));

signals:
    void closed(ChatWidget *widget);

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void closeAnimation();
    void onSendRequested(const QString &text, const QList<QPixmap> &images);
    void onStopRequested();
    void onPinToggled(bool checked);
    void onClearHistoryRequested();
    void onChatResponse(const QString &text);
    void onChatStreamStarted();
    void onChatStreamDelta(const QString &text);
    void onChatStreamFinished(const QString &text);
    void onChatError(const QString &text);
    void onChatRequestStateChanged(bool pending);
    void onUsageAvailable(int inputTokens, int outputTokens, int totalTokens);
    void onWebBridgeReady();
    void onRetryRequested();
    void onToolExecuting(const QString &name, const QString &args);
    void onToolExecuted(const QString &name, const QString &result);
    void onAssistantPrefixFinalized(const QString &text);
    void saveSession();
    void loadSession();

private:
    SettingModel* settings_ = nullptr;
    struct MessageQueueItem {
        QString text;
        QList<QPixmap> images;
    };

    static constexpr int kMaxQueueSize = 5;

    void initializeChatUi();
    QString appendChatMessage(const QString &role, const QString &text, const QList<QPixmap> *images = nullptr, bool completed = true, bool hideBubbleActions = false, const QVariantMap *usage = nullptr);
    void updateChatMessage(const QString &messageId, const QString &role, const QString &text, const QList<QPixmap> *images = nullptr, bool completed = true, bool hideBubbleActions = false, const QVariantMap *usage = nullptr);
    QVariantMap buildMessagePayload(const QString &messageId, const QString &role, const QString &text, const QList<QPixmap> *images = nullptr, bool completed = true, bool hideBubbleActions = false, const QVariantMap *usage = nullptr) const;
    void syncAllMessagesToWeb();
    QString pixmapToDataUrl(const QPixmap &image) const;
    QString nextMessageId();
    bool isDraggableTitleArea(const QPoint &pos) const;
    void applyPinnedState(bool pinned);
    void refreshTitleBarButtons();
    void refreshToolRegistry();
    void enqueueOrSend(const QString &text, const QList<QPixmap> &images = QList<QPixmap>());
    void dispatchMessage(const QString &text, const QList<QPixmap> &images);
    void processNextInQueue();
    void updateQueueDisplay();
    void removeQueuedMessage(int index);
    QWidget *titleBar_;
    QWidget *contentWidget_;
    QVBoxLayout *contentLayout_;
    QLabel *titleLabel_;
    QWebEngineView *messageView_;
    ChatInputWidget *inputWidget_;
    QSplitter *splitter_ = nullptr;
    BrowserPanel *browserPanel_ = nullptr;
    // 首次显示恢复浏览器模式时，在布局激活后再次应用展开时算出的 2:3 宽度。
    int browserPaneWidth_ = -1;
    int chatPaneWidth_ = -1;
    // 首次显示时恢复上次的“联网 / 浏览器”选择，只做一次。
    bool restoredInputMode_ = false;
    // 恢复布局期间为 true：此时窗口几何已经恢复好，打开浏览器不能再加宽窗口。
    bool restoringLayout_ = false;
    QPushButton *closeBtn_;
    QPushButton *pinBtn_;
    QPushButton *clearBtn_;
    QPushButton *maxBtn_ = nullptr;
    bool pinned_;
    Agent::AgentRunner *agent_;
    Agent::ToolRegistry *toolRegistry_;
    LlmTools::BackgroundBrowser *backgroundBrowser_ = nullptr;
    ChatWebBridge *webBridge_;
    bool webReady_;
    QVariantList messages_;
    QString latestAssistantMessage_;
    int nextMessageId_;
    QString streamingMessageId_;
    QString streamingMessageText_;
    QVariantMap latestAssistantUsage_;
    bool dragging_;
    QPoint dragOffset_;
    bool chatRequestPending_;
    QList<MessageQueueItem> messageQueue_;
    QWidget *queuePanel_;
};
