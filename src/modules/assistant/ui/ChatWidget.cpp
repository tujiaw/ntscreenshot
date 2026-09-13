#include "modules/assistant/ui/ChatWidget.h"

#include <QHBoxLayout>
#include <QPointer>
#include <QShowEvent>
#include <QSplitter>
#include <QTimer>
#include <QApplication>
#include <QScreen>
#include "modules/assistant/ui/BrowserPanel.h"
#include "modules/assistant/runtime/tools/BrowserUseTool.h"
#include <QSizePolicy>
#include <QToolButton>
#include <QVBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QBuffer>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QFrame>
#include <QFontMetrics>
#include <QFile>
#include <QIcon>
#include <QKeySequence>
#include <QShortcut>
#include <QStyle>
#include <QDesktopServices>
#include <QList>
#include <QPixmap>
#include <QUrl>
#include <QVariantMap>
#include <QWebChannel>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QWebEngineView>
#include "modules/assistant/runtime/agent/Agent.h"
#include "modules/assistant/runtime/agent/AgentToolRegistry.h"
#include "modules/assistant/runtime/tools/LlmTool.h"
#include "modules/assistant/runtime/tools/BrowserTool.h"
#include "modules/assistant/runtime/tools/WebTools.h"
#include "modules/assistant/ui/ChatSessionStore.h"
#include "core/theme/OverlayTheme.h"
#include "core/platform/Util.h"
#include "core/settings/SettingModel.h"
#include "modules/assistant/ui/ChatInputWidget.h"
#include "modules/assistant/ui/ChatWebBridge.h"

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace {
QColor opaqueColor(const QColor &color)
{
    QColor opaque = color;
    opaque.setAlpha(255);
    return opaque;
}

int chatCornerRadiusPx()
{
    return Util::scaleSize(8);
}

QIcon colorizedIcon(const QString &resourcePath, const QColor &color)
{
    QPixmap src(resourcePath);
    if (src.isNull()) {
        return QIcon();
    }

    QPixmap dst(src.size());
    dst.fill(Qt::transparent);

    QPainter painter(&dst);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawPixmap(0, 0, src);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(dst.rect(), color);
    return QIcon(dst);
}

QString buildTitleButtonStyle(const QString &objectName,
                              const QString &normalBorder,
                              const QString &normalBg,
                              const QString &hoverBorder,
                              const QString &hoverBg,
                              const QString &checkedBorder = QString(),
                              const QString &checkedBg = QString())
{
    Q_UNUSED(normalBorder);
    Q_UNUSED(hoverBorder);
    Q_UNUSED(checkedBorder);
    QString style = QStringLiteral(
        "QPushButton#%1{"
        "border:none;"
        "border-radius:%4px;"
        "background:%2;"
        "padding:0;"
        "}"
        "QPushButton#%1:hover{"
        "background:%3;"
        "}")
        .arg(objectName, normalBg, hoverBg, QString::number(Util::scaleSize(14)));

    if (!checkedBg.isEmpty()) {
        style += QStringLiteral(
        "QPushButton#%1:checked{"
        "background:%2;"
        "}")
            .arg(objectName, checkedBg);
    }
    return style;
}

// 消息角色 key（对应 HTML 侧 CSS class 和 JS role 字段）
const QString kRoleUser      = QStringLiteral("user");
const QString kRoleAssistant = QStringLiteral("assistant");
const QString kRoleError     = QStringLiteral("error");

QString roleLabelFor(const QString &role)
{
    if (role == kRoleUser)      return QStringLiteral("你");
    if (role == kRoleAssistant) return QStringLiteral("助手");
    if (role == kRoleError)     return QStringLiteral("错误");
    return role;
}

bool shouldOpenInExternalBrowser(const QUrl &url)
{
    const QString scheme = url.scheme().toLower();
    return scheme == QStringLiteral("http")
        || scheme == QStringLiteral("https")
        || scheme == QStringLiteral("mailto");
}

class ExternalLinkPage : public QWebEnginePage
{
public:
    explicit ExternalLinkPage(QObject *parent = nullptr)
        : QWebEnginePage(parent)
    {
    }

protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override
    {
        Q_UNUSED(type);
        Q_UNUSED(isMainFrame);
        if (shouldOpenInExternalBrowser(url)) {
            QDesktopServices::openUrl(url);
            return false;
        }
        return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
    }

    QWebEnginePage *createWindow(WebWindowType type) override
    {
        Q_UNUSED(type);
        return new ExternalLinkPage(this);
    }
};
}

ChatWidget::ChatWidget(
    SettingModel* settings,
    const QString &title,
    const QString &message,
    const QSize &windowSize,
    QWidget *parent)
    : FramelessWidget(parent)
    , settings_(settings)
    , titleBar_(nullptr)
    , contentWidget_(nullptr)
    , contentLayout_(nullptr)
    , messageView_(nullptr)
    , inputWidget_(nullptr)
    , pinBtn_(nullptr)
    , closeBtn_(nullptr)
    , clearBtn_(nullptr)
    , agent_(nullptr)
    , toolRegistry_(nullptr)
    , webBridge_(nullptr)
    , webReady_(false)
    , nextMessageId_(0)
    , streamingMessageId_()
    , streamingMessageText_()
    , latestAssistantUsage_()
    , dragging_(false)
    , chatRequestPending_(false)
    , pinned_(false)
    , queuePanel_(nullptr)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setWindowTitle(title);
    setWindowIcon(QIcon(QStringLiteral(":/images/ntscreenshot.ico")));
    setEnableStretch(true);
    setStretchBorderWidth(Util::scaleSize(8));
    setEnableEscClose(false);

    // WA_TranslucentBackground + paintEvent 自绘圆角，不使用 setMask，
    // 避免多边形近似导致的锯齿截断，由系统合成器保证像素级透明。
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
    setAttribute(Qt::WA_DeleteOnClose);

    const QSize scaledWindowSize(Util::scaleSize(windowSize.width()), Util::scaleSize(windowSize.height()));
    setMinimumSize(QSize(Util::scaleSize(420), Util::scaleSize(300)));
    resize(scaledWindowSize.expandedTo(minimumSize()));

    titleBar_ = new QWidget(this);
    titleBar_->setObjectName(QStringLiteral("chatTitleBar"));
    titleBar_->setFixedHeight(Util::scaleSize(40));
    titleBar_->installEventFilter(this);

    QHBoxLayout *titleLayout = new QHBoxLayout(titleBar_);
    titleLayout->setContentsMargins(Util::scaleSize(14), Util::scaleSize(6), Util::scaleSize(10), Util::scaleSize(6));
    titleLayout->setSpacing(Util::scaleSize(6));
    titleLayout->setAlignment(Qt::AlignVCenter);

    titleLabel_ = new QLabel(title, titleBar_);
    titleLabel_->setObjectName(QStringLiteral("notificationTitleLabel"));
    titleLabel_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    titleLabel_->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    pinBtn_ = new QPushButton(titleBar_);
    pinBtn_->setObjectName(QStringLiteral("chatPinButton"));
    pinBtn_->setCheckable(true);
    pinBtn_->setFixedSize(Util::scaleSize(28), Util::scaleSize(28));
    pinBtn_->setIconSize(QSize(Util::scaleSize(18), Util::scaleSize(18)));
    pinBtn_->setCursor(Qt::PointingHandCursor);
    pinBtn_->setToolTip(QStringLiteral("置顶"));
    connect(pinBtn_, &QPushButton::toggled, this, &ChatWidget::onPinToggled);

    clearBtn_ = new QPushButton(titleBar_);
    clearBtn_->setObjectName(QStringLiteral("chatClearButton"));
    clearBtn_->setFixedSize(Util::scaleSize(28), Util::scaleSize(28));
    clearBtn_->setIconSize(QSize(Util::scaleSize(18), Util::scaleSize(18)));
    clearBtn_->setCursor(Qt::PointingHandCursor);
    clearBtn_->setToolTip(QStringLiteral("清空历史"));
    connect(clearBtn_, &QPushButton::clicked, this, &ChatWidget::onClearHistoryRequested);

    maxBtn_ = new QPushButton(titleBar_);
    maxBtn_->setObjectName(QStringLiteral("chatMaximizeButton"));
    maxBtn_->setFixedSize(Util::scaleSize(28), Util::scaleSize(28));
    maxBtn_->setCursor(Qt::PointingHandCursor);
    maxBtn_->setToolTip(QStringLiteral("最大化"));
    maxBtn_->setText(QString());
    maxBtn_->setIconSize(QSize(Util::scaleSize(18), Util::scaleSize(18)));
    connect(maxBtn_, &QPushButton::clicked, this, [this] {
        if (isMaximized()) {
            showNormal();
        } else {
            showMaximized();
        }
    });

    closeBtn_ = new QPushButton(QStringLiteral("✕"), titleBar_);
    closeBtn_->setObjectName(QStringLiteral("notificationCloseButton"));
    closeBtn_->setFixedSize(Util::scaleSize(28), Util::scaleSize(28));
    closeBtn_->setCursor(Qt::PointingHandCursor);
    closeBtn_->setToolTip(QStringLiteral("关闭"));
    closeBtn_->setText(QString());
    closeBtn_->setIconSize(QSize(Util::scaleSize(18), Util::scaleSize(18)));
    connect(closeBtn_, &QPushButton::clicked, this, &ChatWidget::closeAnimation);

    titleLayout->addWidget(titleLabel_, 0, Qt::AlignVCenter);
    titleLayout->addStretch();
    titleLayout->addWidget(pinBtn_, 0, Qt::AlignVCenter);
    titleLayout->addWidget(clearBtn_, 0, Qt::AlignVCenter);
    titleLayout->addWidget(maxBtn_, 0, Qt::AlignVCenter);
    titleLayout->addWidget(closeBtn_, 0, Qt::AlignVCenter);

    setTitle(titleBar_);
    refreshTitleBarButtons();

    auto *escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escShortcut, &QShortcut::activated, this, &ChatWidget::closeAnimation);

    contentWidget_ = new QWidget(this);
    contentLayout_ = new QVBoxLayout(contentWidget_);
    contentLayout_->setContentsMargins(Util::scaleSize(15), 0, Util::scaleSize(15), Util::scaleSize(10));
    contentLayout_->setSpacing(Util::scaleSize(4));

    QFrame *divider = new QFrame(contentWidget_);
    divider->setObjectName(QStringLiteral("notificationDivider"));
    divider->setFrameShape(QFrame::HLine);
    divider->setFrameShadow(QFrame::Plain);
    contentLayout_->addWidget(divider);

    setContent(contentWidget_);
    initializeChatUi();

    if (!message.trimmed().isEmpty()) {
        startChatWithText(message);
    }
}

void ChatWidget::onPinToggled(bool checked)
{
    applyPinnedState(checked);
}

void ChatWidget::onClearHistoryRequested()
{
    if (agent_) {
        agent_->resetConversation();
    }

    messages_.clear();
    messageQueue_.clear();
    latestAssistantMessage_.clear();
    latestAssistantUsage_.clear();
    streamingMessageId_.clear();
    streamingMessageText_.clear();
    nextMessageId_ = 0;
    chatRequestPending_ = false;

    updateQueueDisplay();
    if (inputWidget_) {
        inputWidget_->setPending(false);
    }
    if (webBridge_) {
        emit webBridge_->clearMessages();
        emit webBridge_->requestStateChanged(false);
        emit webBridge_->toolActivity(QStringLiteral("{\"action\":\"clear\"}"));
    }
    ChatSessionStore::clear();
}

ChatWidget::~ChatWidget()
{
    // Stop agent and disconnect BEFORE base class destructors delete child objects.
    // Without this, the worker thread may still emit signals that reach this
    // partially-destroyed widget between our destructor body and ~QObject().
    if (agent_) {
        agent_->stop();
        saveSession();
        agent_->disconnect(this);
        delete agent_;
        agent_ = nullptr;
    }
}

void ChatWidget::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange) {
        refreshTitleBarButtons();
        update();
    } else if (event->type() == QEvent::WindowStateChange) {
        refreshTitleBarButtons();
        update();
    }
    QWidget::changeEvent(event);
}

void ChatWidget::resizeEvent(QResizeEvent *event)
{
    FramelessWidget::resizeEvent(event);

    if (queuePanel_ && queuePanel_->isVisible()) {
        updateQueueDisplay();
    }
}

void ChatWidget::showEvent(QShowEvent *event)
{
    FramelessWidget::showEvent(event);
    // 恢复上次的“联网 / 浏览器”选择。放在首次显示时做而不是构造函数里：
    // 外层是先 new 出窗口再 restoreGeometry，构造期恢复会被随后的几何恢复覆盖掉。
    if (restoredInputMode_ || !inputWidget_ || !settings_) {
        return;
    }
    restoredInputMode_ = true;
    if (!settings_->chatUseBrowser()) {
        return;  // 默认就是“联网”，无需处理
    }
    restoringLayout_ = true;
    inputWidget_->setBrowserEnabled(true);
    restoringLayout_ = false;

    // showEvent 里分栏还没排过版，setSizes 不一定吃得进去；
    // 等第一轮事件循环结束、布局激活之后再摆一次两栏宽度。
    const int chatWidth = chatPaneWidth_;
    const int browserWidth = browserPaneWidth_;
    if (chatWidth <= 0 || browserWidth <= 0) {
        return;
    }
    QPointer<ChatWidget> guard(this);
    QTimer::singleShot(0, this, [guard, chatWidth, browserWidth] {
        if (guard && guard->splitter_ && guard->browserPanel_ && guard->browserPanel_->isVisible()) {
            guard->splitter_->setSizes({chatWidth, browserWidth});
        }
    });
}

bool ChatWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == titleBar_) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton &&
                isDraggableTitleArea(mouseEvent->position().toPoint())) {
                dragging_ = true;
                dragOffset_ = mouseEvent->globalPosition().toPoint() - frameGeometry().topLeft();
                event->accept();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto *mouseEvent = static_cast<QMouseEvent*>(event);
            if ((mouseEvent->buttons() & Qt::LeftButton) == 0) {
                dragging_ = false;
            } else if (dragging_) {
                move(mouseEvent->globalPosition().toPoint() - dragOffset_);
                event->accept();
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            auto *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                dragging_ = false;
            }
        } else if (event->type() == QEvent::Leave) {
            dragging_ = false;
        }
    }

    return FramelessWidget::eventFilter(watched, event);
}

void ChatWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 最大化时铺满整个窗口，避免圆角在半透明背景上留下透明空隙。
    if (isMaximized()) {
        painter.fillRect(rect(), opaqueColor(OverlayTheme::notificationBackgroundColor()));
        return;
    }

    const int radius = chatCornerRadiusPx();
    // 先将整个窗口清为透明，再叠加圆角背景。
    // 依赖 WA_TranslucentBackground + 系统合成器实现像素级圆角，无需 setMask。
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect(), Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    QPainterPath path;
    path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    painter.fillPath(path, OverlayTheme::notificationBackgroundColor());
    painter.setPen(QPen(OverlayTheme::notificationBorderColor(), 1));
    painter.drawPath(path);
}

void ChatWidget::closeAnimation()
{
    emit closed(this);
    close();
}

void ChatWidget::onSendRequested(const QString &text, const QList<QPixmap> &images)
{
    if (!agent_) {
        return;
    }

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty() && images.isEmpty()) {
        return;
    }

    enqueueOrSend(trimmed, images);
}

void ChatWidget::onStopRequested()
{
    if (!agent_ || !chatRequestPending_) {
        return;
    }

    agent_->stop();
}

void ChatWidget::onChatResponse(const QString &text)
{
    appendChatMessage(kRoleAssistant, text, nullptr, true, false, &latestAssistantUsage_);
    latestAssistantMessage_ = text;
    streamingMessageId_.clear();
    streamingMessageText_.clear();
}

void ChatWidget::onChatStreamStarted()
{
    streamingMessageId_.clear();
    streamingMessageText_.clear();
    // 流开始不代表请求完成，不在此重置 pending 状态。
    // JS 侧会在 appendMessage/updateMessage 收到 assistant 消息时自动清除等待动画。
}

void ChatWidget::onChatStreamDelta(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }

    streamingMessageText_ += text;
    latestAssistantMessage_ = streamingMessageText_;

    if (streamingMessageId_.isEmpty()) {
        streamingMessageId_ = appendChatMessage(kRoleAssistant, streamingMessageText_, nullptr, false);
    } else {
        updateChatMessage(streamingMessageId_, kRoleAssistant, streamingMessageText_, nullptr, false);
    }
}

void ChatWidget::onChatStreamFinished(const QString &text)
{
    if (!text.isEmpty()) {
        latestAssistantMessage_ = text;
        if (streamingMessageId_.isEmpty()) {
            streamingMessageId_ = appendChatMessage(kRoleAssistant, text, nullptr, true, false, &latestAssistantUsage_);
        } else {
            updateChatMessage(streamingMessageId_, kRoleAssistant, text, nullptr, true, false, &latestAssistantUsage_);
        }
    }

    streamingMessageId_.clear();
    streamingMessageText_.clear();
}

void ChatWidget::onChatError(const QString &text)
{
    streamingMessageId_.clear();
    streamingMessageText_.clear();
    appendChatMessage(kRoleError, text, nullptr, true);
}

void ChatWidget::onChatRequestStateChanged(bool pending)
{
    chatRequestPending_ = pending;

    if (inputWidget_) {
        inputWidget_->setPending(pending);
    }

    if (webBridge_) {
        emit webBridge_->requestStateChanged(pending);
    }

    if (!pending) {
        processNextInQueue();
    }
}

void ChatWidget::onUsageAvailable(int inputTokens, int outputTokens, int totalTokens)
{
    latestAssistantUsage_.clear();
    if (inputTokens >= 0) {
        latestAssistantUsage_.insert(QStringLiteral("inputTokens"), inputTokens);
    }
    if (outputTokens >= 0) {
        latestAssistantUsage_.insert(QStringLiteral("outputTokens"), outputTokens);
    }
    if (totalTokens >= 0) {
        latestAssistantUsage_.insert(QStringLiteral("totalTokens"), totalTokens);
    }
}

void ChatWidget::initializeChatUi()
{
    if (!contentLayout_) {
        return;
    }

    // 恢复上次的两栏宽度；存过 0 或没存过时保持 -1，走默认宽度。
    if (settings_) {
        if (const int width = settings_->chatPaneWidth(); width > 0) chatPaneWidth_ = width;
        if (const int width = settings_->chatBrowserPaneWidth(); width > 0) browserPaneWidth_ = width;
    }

    auto *splitter = new QSplitter(Qt::Horizontal, contentWidget_);
    splitter_ = splitter;
    splitter->setChildrenCollapsible(false);
    auto *chatPane = new QWidget(splitter);
    auto *chatLayout = new QVBoxLayout(chatPane);
    chatLayout->setContentsMargins(0,0,0,0);
    browserPanel_ = new BrowserPanel(splitter);
    splitter->addWidget(chatPane);
    splitter->addWidget(browserPanel_);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    browserPanel_->hide();
    contentLayout_->addWidget(splitter, 1);
    // 分栏被拖动时记住两栏宽度；面板隐藏时左栏铺满整个分栏，那种“宽度”没有意义。
    connect(splitter, &QSplitter::splitterMoved, this, [this, splitter] {
        if (!browserPanel_ || !browserPanel_->isVisible() || !settings_) return;
        const QList<int> sizes = splitter->sizes();
        settings_->setChatPaneWidth(sizes.value(0));
        settings_->setChatBrowserPaneWidth(sizes.value(1));
    });
    refreshTitleBarButtons();

    messageView_ = new QWebEngineView(chatPane);
    messageView_->setObjectName(QStringLiteral("notificationChatView"));
    messageView_->setPage(new ExternalLinkPage(messageView_));
    // 允许 HTML 消息区域使用 WebEngine 标准右键菜单，例如复制、全选、链接操作等。
    messageView_->setContextMenuPolicy(Qt::DefaultContextMenu);
    messageView_->page()->setBackgroundColor(opaqueColor(OverlayTheme::notificationBackgroundColor()));
    messageView_->setZoomFactor(Util::getScreenScaleFactor());
    messageView_->settings()->setAttribute(QWebEngineSettings::ShowScrollBars, true);
    messageView_->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);
    messageView_->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);

    auto *channel = new QWebChannel(messageView_->page());
    webBridge_ = new ChatWebBridge(this);
    channel->registerObject(QStringLiteral("chatBridge"), webBridge_);
    messageView_->page()->setWebChannel(channel);
    connect(webBridge_, &ChatWebBridge::sigReady, this, &ChatWidget::onWebBridgeReady);
    connect(webBridge_, &ChatWebBridge::sigRetryRequested, this, &ChatWidget::onRetryRequested);

    QFile htmlFile(QStringLiteral(":/html/chat.html"));
    if (htmlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString html = QString::fromUtf8(htmlFile.readAll());
        messageView_->setHtml(html, QUrl(QStringLiteral("qrc:/html/")));
    } else {
        messageView_->setHtml(QStringLiteral("<html><body>Failed to load chat.html</body></html>"));
    }
    chatLayout->addWidget(messageView_, 1);

    // 队列预览面板（有待发送消息时显示，紧靠输入框上方）
    queuePanel_ = new QWidget(contentWidget_);
    auto *queueLayout = new QVBoxLayout(queuePanel_);
    queueLayout->setContentsMargins(Util::scaleSize(10), Util::scaleSize(5), Util::scaleSize(10), Util::scaleSize(2));
    queueLayout->setSpacing(Util::scaleSize(2));
    queuePanel_->setVisible(false);
    chatLayout->addWidget(queuePanel_);

    inputWidget_ = new ChatInputWidget(settings_, contentWidget_);
    // inputWidget_->setPlaceholderText("问问AI");
    connect(inputWidget_, &ChatInputWidget::sigSendRequested, this, &ChatWidget::onSendRequested);
    connect(inputWidget_, &ChatInputWidget::sigStopRequested, this, &ChatWidget::onStopRequested);
    chatLayout->addWidget(inputWidget_);

    agent_ = new Agent::AgentRunner(settings_, this);
    backgroundBrowser_ = new LlmTools::BackgroundBrowser(this);
    // 打开浏览器时窗口整体加宽，关闭时再收回去：对话区域宽度全程不变，
    // 不会出现“关掉浏览器后对话区域横向铺满”的情况。
    connect(inputWidget_, &ChatInputWidget::sigBrowserEnabledChanged, this, [this, splitter](bool visible) {
        const int minBrowserWidth = Util::scaleSize(320);
        // 两个面板之间会多出一个分栏手柄，增删面板时窗口宽度要把它的宽度算进去。
        const int handle = splitter->handleWidth();
        const QList<int> sizes = splitter->sizes();
        if (visible) {
            int browserWidth = browserPaneWidth_ > 0 ? browserPaneWidth_ : Util::scaleSize(480);
            const QRect available = screen() ? screen()->availableGeometry() : QRect();
            if (!isMaximized() && available.isValid()) {
                // 只向右扩展到屏幕边界，空间不足时压缩浏览器而不是挤压对话区域。
                const int room = available.right() - x() + 1 - width() - handle;
                browserWidth = qBound(minBrowserWidth, browserWidth, qMax(minBrowserWidth, room));
            }
            // 对话区域当前宽度：面板隐藏时它占满整个分栏。
            // 启动恢复时优先用上次记住的宽度，让两栏都回到关闭前的样子。
            const int chatWidth = (restoringLayout_ && chatPaneWidth_ > 0)
                ? chatPaneWidth_ : qMax(1, sizes.value(0));
            browserPanel_->setVisible(true);
            if (restoringLayout_) {
                // 窗口几何已经由 restoreGeometry 恢复过了，这里只摆好两栏，
                // 不能再走加宽逻辑，否则每启动一次窗口就宽一截。
                splitter->setSizes({chatWidth, browserWidth});
            } else if (isMaximized()) {
                splitter->setSizes({qMax(1, splitter->width() - browserWidth - handle), browserWidth});
            } else {
                resize(width() + browserWidth + handle, height());
                if (available.isValid()) {
                    move(qBound(available.left(), x(),
                                qMax(available.left(), available.right() - width() + 1)), y());
                }
                splitter->setSizes({chatWidth, browserWidth});
            }
            browserPaneWidth_ = browserWidth;
            if (settings_) {
                settings_->setChatUseBrowser(true);
                settings_->setChatBrowserPaneWidth(browserWidth);
                if (chatWidth > 0) settings_->setChatPaneWidth(chatWidth);
            }
        } else {
            const int browserWidth = sizes.value(1);
            browserPanel_->setVisible(false);
            if (browserWidth > 0) {
                browserPaneWidth_ = browserWidth;
                if (!isMaximized()) {  // 收回浏览器占用的宽度，对话区域宽度保持不变
                    resize(qMax(minimumWidth(), width() - browserWidth - handle), height());
                }
                if (settings_) {
                    settings_->setChatBrowserPaneWidth(browserWidth);
                    if (sizes.value(0) > 0) settings_->setChatPaneWidth(sizes.value(0));
                }
            }
            if (settings_) settings_->setChatUseBrowser(false);
        }
        refreshToolRegistry();
    });
    connect(inputWidget_, &ChatInputWidget::sigWebEnabledChanged, this, [this](bool) {
        refreshToolRegistry();  // 两个按钮的互斥已在 ChatInputWidget 内处理
    });
    connect(browserPanel_, &BrowserPanel::activityRequested, this, [this]{ inputWidget_->setBrowserEnabled(true); });
    connect(browserPanel_, &BrowserPanel::attentionRequired, this, [this](const QString &reason){
        inputWidget_->setBrowserEnabled(true);
        appendChatMessage(kRoleAssistant, reason);
        QApplication::alert(this);
    });
    connect(browserPanel_, &BrowserPanel::collapseRequested, this, [this]{ inputWidget_->setBrowserEnabled(false); });
    connect(inputWidget_, &ChatInputWidget::sigStopRequested, browserPanel_, &BrowserPanel::cancel);
    refreshToolRegistry();
    agent_->setMaxIterations(10);

    connect(agent_, &Agent::AgentRunner::sigStreamStarted, this, &ChatWidget::onChatStreamStarted);
    connect(agent_, &Agent::AgentRunner::sigStreamDelta, this, &ChatWidget::onChatStreamDelta);
    connect(agent_, &Agent::AgentRunner::sigStreamFinished, this, &ChatWidget::onChatStreamFinished);
    connect(agent_, &Agent::AgentRunner::sigError, this, &ChatWidget::onChatError);
    connect(agent_, &Agent::AgentRunner::sigStateChanged, this, &ChatWidget::onChatRequestStateChanged);
    connect(agent_, &Agent::AgentRunner::sigUsageAvailable, this, &ChatWidget::onUsageAvailable);
    connect(agent_, &Agent::AgentRunner::sigToolExecuting, this, &ChatWidget::onToolExecuting);
    connect(agent_, &Agent::AgentRunner::sigToolExecuted, this, &ChatWidget::onToolExecuted);
    connect(agent_, &Agent::AgentRunner::sigAssistantPrefixFinalized, this, &ChatWidget::onAssistantPrefixFinalized);
    connect(agent_, &Agent::AgentRunner::sigSessionChanged, this, &ChatWidget::saveSession);
    loadSession();
}

void ChatWidget::refreshToolRegistry()
{
    if (!agent_ || !inputWidget_) {
        return;
    }
    if (chatRequestPending_) {
        return;
    }

    // 联网（后台搜索/读取）与浏览器操作（右侧可见浏览器）二选一，互斥。
    // 前者只注册后台浏览器工具，后者只注册 browser_use 工具。
    Agent::ToolRegistry *registry = new Agent::ToolRegistry(this);
    if (inputWidget_->browserEnabled()) {
        registry->registerTool(QSharedPointer<LlmTools::LlmTool>(new LlmTools::BrowserUseTool(browserPanel_)));
    }
    if (inputWidget_->webEnabled()) {
        registry->registerTool(QSharedPointer<LlmTools::LlmTool>(new LlmTools::BrowserTool(backgroundBrowser_, true)));
        registry->registerTool(QSharedPointer<LlmTools::LlmTool>(new LlmTools::BrowserTool(backgroundBrowser_, false)));
        registry->registerTool(QSharedPointer<LlmTools::LlmTool>(new LlmTools::RunJavaScriptTool(backgroundBrowser_)));
        registry->registerTool(QSharedPointer<LlmTools::LlmTool>(new LlmTools::ReadArticleTool(backgroundBrowser_)));
    }

    Agent::ToolRegistry *oldRegistry = toolRegistry_;
    toolRegistry_ = registry;
    agent_->setToolRegistry(toolRegistry_);
    if (oldRegistry) {
        oldRegistry->deleteLater();
    }
}

QString ChatWidget::appendChatMessage(const QString &role, const QString &text, const QList<QPixmap> *images, bool completed, bool hideBubbleActions, const QVariantMap *usage)
{
    const QString messageId = nextMessageId();
    const QVariantMap payload = buildMessagePayload(messageId, role, text, images, completed, hideBubbleActions, usage);
    messages_.append(payload);

    if (webReady_ && webBridge_) {
        const QString json = QString::fromUtf8(QJsonDocument::fromVariant(payload).toJson(QJsonDocument::Compact));
        emit webBridge_->appendMessage(json);
    }

    return messageId;
}

void ChatWidget::updateChatMessage(const QString &messageId, const QString &role, const QString &text, const QList<QPixmap> *images, bool completed, bool hideBubbleActions, const QVariantMap *usage)
{
    if (messageId.isEmpty()) {
        return;
    }

    const QVariantMap payload = buildMessagePayload(messageId, role, text, images, completed, hideBubbleActions, usage);
    for (int i = 0; i < messages_.size(); ++i) {
        const QVariantMap existing = messages_.at(i).toMap();
        if (existing.value(QStringLiteral("id")).toString() == messageId) {
            messages_[i] = payload;
            if (webReady_ && webBridge_) {
                const QString json = QString::fromUtf8(QJsonDocument::fromVariant(payload).toJson(QJsonDocument::Compact));
                emit webBridge_->updateMessage(json);
            }
            return;
        }
    }
}

QString ChatWidget::pixmapToDataUrl(const QPixmap &image) const
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return QStringLiteral("data:image/png;base64,%1").arg(QString::fromLatin1(bytes.toBase64()));
}

bool ChatWidget::isDraggableTitleArea(const QPoint &pos) const
{
    if (!closeBtn_ || !pinBtn_ || !clearBtn_ || !maxBtn_) {
        return false;
    }

    const int titleBottom = closeBtn_->geometry().bottom() + 6;
    if (pos.y() > titleBottom) {
        return false;
    }

    if (closeBtn_->geometry().contains(pos) ||
        pinBtn_->geometry().contains(pos) ||
        clearBtn_->geometry().contains(pos) ||
        maxBtn_->geometry().contains(pos)) {
        return false;
    }

    return true;
}

void ChatWidget::applyPinnedState(bool pinned)
{
    pinned_ = pinned;
    if (pinBtn_) {
        pinBtn_->setToolTip(pinned_
            ? QStringLiteral("取消置顶")
            : QStringLiteral("置顶窗口"));
        if (pinBtn_->isChecked() != pinned_) {
            pinBtn_->setChecked(pinned_);
        }
    }

#ifdef Q_OS_WIN
    if (HWND hwnd = reinterpret_cast<HWND>(winId())) {
        const UINT flags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOACTIVATE;
        ::SetWindowPos(hwnd, pinned_ ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, flags);
    }
#else
    const QPoint currentPos = pos();
    const bool wasVisible = isVisible();
    setWindowFlag(Qt::WindowStaysOnTopHint, pinned_);
    if (wasVisible) {
        show();
        move(currentPos);
    }
#endif

    if (pinned_ && isVisible()) {
        raise();
    }

    refreshTitleBarButtons();
}

void ChatWidget::refreshTitleBarButtons()
{
    if (!pinBtn_ || !clearBtn_ || !closeBtn_ || !maxBtn_) {
        return;
    }

    const bool dark = OverlayTheme::isDarkTheme();
    const QColor iconColor = dark ? QColor("#f3f4f6") : QColor("#111827");
    const QColor accentColor = dark ? QColor("#93c5fd") : QColor("#2563eb");
    const QString normalBorder = dark ? "rgba(255,255,255,0.12)" : "rgba(15,23,42,0.10)";
    const QString normalBg = dark ? "rgba(255,255,255,0.05)" : "rgba(255,255,255,0.92)";
    const QString hoverBorder = dark ? "rgba(255,255,255,0.18)" : "rgba(15,23,42,0.16)";
    const QString hoverBg = dark ? "rgba(255,255,255,0.10)" : "rgba(15,23,42,0.06)";

    pinBtn_->setIcon(colorizedIcon(QStringLiteral(":/images/pin.png"), pinned_ ? accentColor : iconColor));
    pinBtn_->setStyleSheet(buildTitleButtonStyle(
        QStringLiteral("chatPinButton"),
        normalBorder,
        normalBg,
        hoverBorder,
        hoverBg,
        dark ? "rgba(147,197,253,0.48)" : "rgba(37,99,235,0.24)",
        dark ? "rgba(59,130,246,0.18)" : "rgba(37,99,235,0.12)"));

    clearBtn_->setIcon(colorizedIcon(QStringLiteral(":/images/clear.png"), iconColor));
    clearBtn_->setStyleSheet(buildTitleButtonStyle(
        QStringLiteral("chatClearButton"),
        normalBorder,
        normalBg,
        dark ? "rgba(251,191,36,0.48)" : "rgba(217,119,6,0.20)",
        dark ? "rgba(245,158,11,0.18)" : "rgba(245,158,11,0.10)"));

    closeBtn_->setIcon(colorizedIcon(QStringLiteral(":/images/remove.png"), iconColor));
    closeBtn_->setStyleSheet(buildTitleButtonStyle(
        QStringLiteral("notificationCloseButton"),
        normalBorder,
        normalBg,
        dark ? "rgba(248,113,113,0.48)" : "rgba(220,38,38,0.18)",
        dark ? "rgba(239,68,68,0.18)" : "rgba(239,68,68,0.10)"));

    maxBtn_->setIcon(colorizedIcon(isMaximized()
        ? QStringLiteral(":/images/icon_window_restore.png")
        : QStringLiteral(":/images/icon_window_maximize.png"), iconColor));
    maxBtn_->setToolTip(isMaximized() ? QStringLiteral("还原") : QStringLiteral("最大化"));
    maxBtn_->setStyleSheet(buildTitleButtonStyle(
        QStringLiteral("chatMaximizeButton"),
        normalBorder,
        normalBg,
        hoverBorder,
        hoverBg));
}

QVariantMap ChatWidget::buildMessagePayload(const QString &messageId, const QString &role, const QString &text, const QList<QPixmap> *images, bool completed, bool hideBubbleActions, const QVariantMap *usage) const
{
    QVariantMap payload;
    payload.insert(QStringLiteral("id"), messageId);
    payload.insert(QStringLiteral("role"), role);
    payload.insert(QStringLiteral("roleLabel"), roleLabelFor(role));
    payload.insert(QStringLiteral("text"), text);
    payload.insert(QStringLiteral("completed"), completed);
    if (usage && !usage->isEmpty()) {
        payload.insert(QStringLiteral("usage"), *usage);
    }
    if (hideBubbleActions) {
        payload.insert(QStringLiteral("hideBubbleActions"), true);
    }

    if (images && !images->isEmpty()) {
        QVariantList imageDataUrls;
        for (const QPixmap &image : *images) {
            if (!image.isNull()) {
                imageDataUrls.append(pixmapToDataUrl(image));
            }
        }

        if (!imageDataUrls.isEmpty()) {
            payload.insert(QStringLiteral("imageDataUrls"), imageDataUrls);
            payload.insert(QStringLiteral("imageDataUrl"), imageDataUrls.first());
        }
    }

    return payload;
}

void ChatWidget::syncAllMessagesToWeb()
{
    if (!webReady_ || !webBridge_) {
        return;
    }

    const QString json = QString::fromUtf8(QJsonDocument::fromVariant(messages_).toJson(QJsonDocument::Compact));
    emit webBridge_->hydrateMessages(json);
}

void ChatWidget::startChatWithText(const QString &text)
{
    if (!agent_) {
        return;
    }

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    enqueueOrSend(trimmed);
}

void ChatWidget::startChatWithImage(const QString &text, const QPixmap &image)
{
    if (!agent_ || image.isNull()) {
        return;
    }

    enqueueOrSend(text.trimmed(), QList<QPixmap>{image});
}

void ChatWidget::enqueueOrSend(const QString &text, const QList<QPixmap> &images)
{
    if (!chatRequestPending_) {
        dispatchMessage(text, images);
        return;
    }

    if (messageQueue_.size() >= kMaxQueueSize) {
        return;
    }

    messageQueue_.append({text, images});
    updateQueueDisplay();
}

void ChatWidget::dispatchMessage(const QString &text, const QList<QPixmap> &images)
{
    latestAssistantUsage_.clear();
    refreshToolRegistry();
    if (!images.isEmpty()) {
        appendChatMessage(kRoleUser, text, &images);
        agent_->runWithImages(text, images);
    } else {
        appendChatMessage(kRoleUser, text);
        agent_->run(text);
    }
}

void ChatWidget::processNextInQueue()
{
    if (messageQueue_.isEmpty()) {
        return;
    }

    const MessageQueueItem item = messageQueue_.takeFirst();
    updateQueueDisplay();
    dispatchMessage(item.text, item.images);
}

void ChatWidget::removeQueuedMessage(int index)
{
    if (index < 0 || index >= messageQueue_.size()) {
        return;
    }
    messageQueue_.removeAt(index);
    updateQueueDisplay();
}

void ChatWidget::quoteTextInInput(const QString &text)
{
    if (!inputWidget_) {
        return;
    }
    inputWidget_->quoteText(text);
}

void ChatWidget::quoteImageInInput(const QPixmap &image, const QString &displayName)
{
    if (!inputWidget_ || image.isNull()) {
        return;
    }
    inputWidget_->quoteImage(image, displayName);
}

void ChatWidget::updateQueueDisplay()
{
    if (!queuePanel_) {
        return;
    }

    auto *layout = qobject_cast<QVBoxLayout *>(queuePanel_->layout());
    if (!layout) {
        return;
    }

    // 清空旧内容
    while (layout->count() > 0) {
        QLayoutItem *li = layout->takeAt(0);
        delete li->widget();
        delete li;
    }

    if (messageQueue_.isEmpty()) {
        queuePanel_->setVisible(false);
        return;
    }

    const bool dark = OverlayTheme::isDarkTheme();
    const QString headerColor = dark ? "#9ca3af" : "#6b7280";
    const QString itemBg      = dark ? "rgba(255,255,255,0.05)" : "rgba(0,0,0,0.04)";
    const QString itemColor   = dark ? "#d1d5db" : "#374151";

    // 标题行：显示当前队列深度
    auto *header = new QLabel(
        QStringLiteral("待发送  %1 / %2").arg(messageQueue_.size()).arg(kMaxQueueSize),
        queuePanel_);
    header->setStyleSheet(QStringLiteral("font-size:%1px; color:%2;")
        .arg(Util::scaleSize(11))
        .arg(headerColor));
    layout->addWidget(header);

    // 可用宽度（父容器宽度减去左右 margin 和序号区域）
    const int panelW = qMax(200, contentWidget_ ? contentWidget_->width() - 44 : 300);
    const QFontMetrics fm{QFont()};

    for (int i = 0; i < messageQueue_.size(); ++i) {
        const auto &item = messageQueue_.at(i);
        const bool hasImages = !item.images.isEmpty();
        const QString imagePrefix = hasImages
            ? (item.images.size() == 1
                ? QStringLiteral("[图片]")
                : QStringLiteral("[图片x%1]").arg(item.images.size()))
            : QString();
        const QString raw = !hasImages
            ? item.text
            : (item.text.isEmpty() ? imagePrefix
                                   : QStringLiteral("%1 %2").arg(imagePrefix, item.text));

        const QString elided = fm.elidedText(raw, Qt::ElideRight, panelW - 28);

        auto *rowWidget = new QWidget(queuePanel_);
        auto *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(6);

        auto *rowLabel = new QLabel(queuePanel_);
        rowLabel->setText(QStringLiteral("%1. %2").arg(i + 1).arg(elided));
        rowLabel->setStyleSheet(QStringLiteral(
            "font-size:%1px; color:%2;"
            "background:%3;"
            "border-radius:%4px;"
            "padding:%5px %6px;"
        ).arg(Util::scaleSize(12))
         .arg(itemColor, itemBg)
         .arg(Util::scaleSize(4))
         .arg(Util::scaleSize(2))
         .arg(Util::scaleSize(6)));
        rowLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

        auto *removeBtn = new QToolButton(queuePanel_);
        removeBtn->setObjectName(QStringLiteral("chatQueueRemoveButton"));
        removeBtn->setText(QStringLiteral("×"));
        removeBtn->setCursor(Qt::PointingHandCursor);
        removeBtn->setFocusPolicy(Qt::NoFocus);
        removeBtn->setToolTip(QStringLiteral("从队列移除"));
        removeBtn->setProperty("queueIndex", i);
        connect(removeBtn, &QToolButton::clicked, this, [this]() {
            auto *btn = qobject_cast<QToolButton *>(sender());
            if (!btn) {
                return;
            }
            const int idx = btn->property("queueIndex").toInt();
            removeQueuedMessage(idx);
        });

        rowLayout->addWidget(rowLabel, 1);
        rowLayout->addWidget(removeBtn, 0);
        layout->addWidget(rowWidget);
    }

    queuePanel_->setVisible(true);
}

QString ChatWidget::nextMessageId()
{
    ++nextMessageId_;
    return QStringLiteral("msg_%1").arg(nextMessageId_);
}

void ChatWidget::onRetryRequested()
{
    if (chatRequestPending_ || !agent_) {
        return;
    }

    for (int i = messages_.size() - 1; i >= 0; --i) {
        const QString role = messages_.at(i).toMap().value(QStringLiteral("role")).toString();
        if (role == kRoleAssistant || role == kRoleError) {
            const QString msgId = messages_.at(i).toMap().value(QStringLiteral("id")).toString();
            messages_.removeAt(i);
            if (webReady_ && webBridge_) {
                emit webBridge_->removeMessage(msgId);
            }
            break;
        }
    }

    latestAssistantMessage_.clear();
    latestAssistantUsage_.clear();
    refreshToolRegistry();
    agent_->retryLastResponse();
}

void ChatWidget::onWebBridgeReady()
{
    webReady_ = true;
    syncAllMessagesToWeb();
    if (webBridge_) {
        emit webBridge_->requestStateChanged(chatRequestPending_);
    }
}

void ChatWidget::onToolExecuting(const QString &name, const QString &args)
{
    if (!webReady_ || !webBridge_) {
        return;
    }
    QJsonObject o;
    o.insert(QStringLiteral("action"), QStringLiteral("executing"));
    o.insert(QStringLiteral("name"), name);
    o.insert(QStringLiteral("args"), args);
    emit webBridge_->toolActivity(QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)));
}

void ChatWidget::onToolExecuted(const QString &name, const QString &result)
{
    if (!webReady_ || !webBridge_) {
        return;
    }
    QJsonObject o;
    o.insert(QStringLiteral("action"), QStringLiteral("executed"));
    o.insert(QStringLiteral("name"), name);
    o.insert(QStringLiteral("result"), result);
    emit webBridge_->toolActivity(QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)));
}

void ChatWidget::onAssistantPrefixFinalized(const QString &text)
{
    if (streamingMessageId_.isEmpty()) {
        return;
    }
    const QString useText = text.isEmpty() ? streamingMessageText_ : text;
    updateChatMessage(streamingMessageId_, kRoleAssistant, useText, nullptr, true, true);
    streamingMessageId_.clear();
    streamingMessageText_.clear();
}

void ChatWidget::saveSession()
{
    if (!agent_) {
        return;
    }
    ChatSessionStore::Session session;
    session.conversationMessages = agent_->conversationSnapshot();
    session.summaryText = agent_->summarySnapshot();
    session.uiMessages = messages_;
    session.nextMessageId = nextMessageId_;
    ChatSessionStore::save(session);
}

void ChatWidget::loadSession()
{
    ChatSessionStore::Session session;
    if (!ChatSessionStore::load(&session)) {
        return;
    }
    if (session.uiMessages.isEmpty() && session.conversationMessages.isEmpty()) {
        return;
    }
    messages_ = session.uiMessages;
    nextMessageId_ = session.nextMessageId;
    if (agent_) {
        agent_->restoreSession(session.conversationMessages, session.summaryText);
    }
    if (webReady_) {
        syncAllMessagesToWeb();
    }
}
