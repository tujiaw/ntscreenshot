#include "modules/assistant/ui/ChatInputWidget.h"

#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QImage>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMimeData>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollArea>
#include <QSize>
#include <QSizePolicy>
#include <QTextDocument>
#include <QTextOption>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "core/platform/Util.h"
#include "core/settings/SettingModel.h"
#include "core/theme/ThemeIcon.h"

namespace {

constexpr int kChatInputToolbarIconPx = 20;
// 联网 / 浏览器两个开关：图标铺满整个按钮，不留任何内边距
// （modernstyle.qss 里必须保持 padding: 0px，否则会被全局规则的
// padding: 4px 10px 挤成中间一小块）。
constexpr int kChatInputTogglePx = 18;
// 两个开关之间额外留的间隔（工具栏本身的 spacing 是两个 18px 图标挨在一起时不够用）
constexpr int kChatInputToggleGapPx = 10;
constexpr int kChatInputMinVisibleLines = 1;
constexpr int kChatInputMaxVisibleLines = 6;

// 发送/停止图标用主题重着色为白色，保证在 accent 圆形按钮上清晰可见
QIcon chatSendIcon()
{
    return ThemeIcon::icon(QStringLiteral("send.png"), IconTone::OnAccent,
                           kChatInputToolbarIconPx);
}

QIcon chatStopIcon()
{
    return ThemeIcon::icon(QStringLiteral("stop.png"), IconTone::OnAccent,
                           kChatInputToolbarIconPx);
}

// 图标 PNG 四周留了透明边，直接缩放会在按钮里显得小一圈。先裁到不透明区域，
// 图片才能铺满按钮。
QPixmap trimTransparentBorder(const QPixmap &source)
{
    const QImage image = source.toImage();
    if (image.isNull()) {
        return source;
    }
    int left = image.width(), top = image.height(), right = -1, bottom = -1;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(image.pixel(x, y)) > 8) {
                left = qMin(left, x);
                right = qMax(right, x);
                top = qMin(top, y);
                bottom = qMax(bottom, y);
            }
        }
    }
    if (right < left || bottom < top) {
        return source;
    }
    return source.copy(left, top, right - left + 1, bottom - top + 1);
}

// 开关图标的配色跟随勾选状态：开启用强调色，关闭用次要色。
QIcon chatToggleIcon(const QString &name, bool enabled)
{
    QPixmap pixmap = trimTransparentBorder(QPixmap(QStringLiteral(":/images/") + name));
    if (pixmap.isNull()) {
        return QIcon();
    }
    const int side = Util::scaleSize(kChatInputTogglePx);
    pixmap = pixmap.scaled(side, side, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    const ThemeTokens &tokens = ThemeManager::tokens();
    QPainter painter(&pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), enabled ? tokens.accent : tokens.textSecondary);
    painter.end();
    QIcon icon(pixmap);
    if (enabled) {
        // 生成期间开关禁用，但仍须显示当前模式，避免 Qt 自动生成灰色图标。
        icon.addPixmap(pixmap, QIcon::Disabled, QIcon::On);
        icon.addPixmap(pixmap, QIcon::Disabled, QIcon::Off);
    }
    return icon;
}

} // namespace

QString normalizedPreviewText(const QString &text)
{
    QString preview = text;
    preview.replace('\n', QChar(' '));
    preview.replace('\r', QChar(' '));
    return preview.simplified();
}

class ElidedPushButton : public QPushButton
{
public:
    explicit ElidedPushButton(QWidget *parent = nullptr)
        : QPushButton(parent)
    {
    }

    void setFullText(const QString &text)
    {
        fullText_ = text;
        updateElidedText();
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QPushButton::resizeEvent(event);
        updateElidedText();
    }

    void changeEvent(QEvent *event) override
    {
        QPushButton::changeEvent(event);
        if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange) {
            updateElidedText();
        }
    }

private:
    void updateElidedText()
    {
        const int iconSpacing = icon().isNull() ? 0 : (iconSize().width() + 2);
        const int availableTextWidth = qMax(0, contentsRect().width() - iconSpacing - 1);
        QPushButton::setText(fontMetrics().elidedText(fullText_, Qt::ElideRight, availableTextWidth));
    }

    QString fullText_;
};

QIcon createQuotedImageThumbnail(const QPixmap &image)
{
    if (image.isNull()) {
        return QIcon();
    }

    const QSize targetSize(Util::scaleSize(18), Util::scaleSize(18));
    const QPixmap scaled = image.scaled(targetSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    const QRect cropRect(
        qMax(0, (scaled.width() - targetSize.width()) / 2),
        qMax(0, (scaled.height() - targetSize.height()) / 2),
        qMin(targetSize.width(), scaled.width()),
        qMin(targetSize.height(), scaled.height()));
    return QIcon(scaled.copy(cropRect));
}

void showTextPreviewDialog(QWidget *parent, const QString &text)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("预览引用文本"));
    dialog.resize(Util::scaleSize(520), Util::scaleSize(320));

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(Util::scaleSize(12), Util::scaleSize(12), Util::scaleSize(12), Util::scaleSize(12));
    layout->setSpacing(Util::scaleSize(8));

    auto *editor = new QPlainTextEdit(&dialog);
    editor->setReadOnly(true);
    editor->setPlainText(text);
    editor->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout->addWidget(editor);

    dialog.exec();
}

void showImagePreviewDialog(QWidget *parent, const QPixmap &image, const QString &title)
{
    if (image.isNull()) {
        return;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(title.isEmpty() ? QStringLiteral("预览引用图片") : title);

    QRect availableGeometry;
    if (const QScreen *screen = QApplication::primaryScreen()) {
        availableGeometry = screen->availableGeometry();
    }
    const QSize dialogSize(
        qBound(Util::scaleSize(420), availableGeometry.width() * 3 / 5, Util::scaleSize(900)),
        qBound(Util::scaleSize(320), availableGeometry.height() * 3 / 5, Util::scaleSize(700)));
    dialog.resize(dialogSize);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(Util::scaleSize(12), Util::scaleSize(12), Util::scaleSize(12), Util::scaleSize(12));
    layout->setSpacing(Util::scaleSize(8));

    auto *scrollArea = new QScrollArea(&dialog);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto *imageLabel = new QLabel(scrollArea);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setPixmap(image);
    imageLabel->setMinimumSize(image.size());
    scrollArea->setWidget(imageLabel);

    layout->addWidget(scrollArea);
    dialog.exec();
}
ChatInputWidget::ChatInputWidget(SettingModel* settings, QWidget *parent)
    : QWidget(parent)
    , settings_(settings)
    , quoteContainer_(nullptr)
    , quoteLayout_(nullptr)
    , inputEdit_(nullptr)
    , addButton_(nullptr)
    , modelButton_(nullptr)
    , sendButton_(nullptr)
    , pending_(false)
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    auto *surface = new QFrame(this);
    surface->setObjectName(QStringLiteral("chatInputSurface"));

    auto *shadow = new QGraphicsDropShadowEffect(surface);
    shadow->setBlurRadius(Util::scaleSize(24));
    shadow->setOffset(0, Util::scaleSize(4));
    shadow->setColor(QColor(15, 23, 42, 24));
    surface->setGraphicsEffect(shadow);

    auto *surfaceLayout = new QVBoxLayout(surface);
    surfaceLayout->setContentsMargins(Util::scaleSize(12), Util::scaleSize(5), Util::scaleSize(10), Util::scaleSize(5));
    surfaceLayout->setSpacing(Util::scaleSize(3));

    quoteContainer_ = new QWidget(surface);
    quoteContainer_->setObjectName(QStringLiteral("chatQuoteContainer"));
    quoteLayout_ = new QHBoxLayout(quoteContainer_);
    quoteLayout_->setContentsMargins(0, 0, 0, 0);
    quoteLayout_->setSpacing(Util::scaleSize(6));
    quoteLayout_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    quoteContainer_->hide();
    surfaceLayout->addWidget(quoteContainer_);

    inputEdit_ = new QPlainTextEdit(surface);
    inputEdit_->setObjectName(QStringLiteral("chatInputEditor"));
    inputEdit_->setAcceptDrops(false);
    inputEdit_->setFrameShape(QFrame::NoFrame);
    inputEdit_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    inputEdit_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    inputEdit_->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    inputEdit_->setTabChangesFocus(true);
    {
        const QFontMetrics fm(inputEdit_->font());
        const int singleLineHeight = qMax(Util::scaleSize(28), fm.lineSpacing() + Util::scaleSize(10));
        inputEdit_->setMinimumHeight(singleLineHeight);
    }
    inputEdit_->installEventFilter(this);
    connect(inputEdit_, &QPlainTextEdit::textChanged, this, [this]() {
        adjustInputHeight();
        updateSendButtonState();
    });
    surfaceLayout->addWidget(inputEdit_);

    auto *toolbarLayout = new QHBoxLayout();
    toolbarLayout->setContentsMargins(0, 0, 0, 0);
    toolbarLayout->setSpacing(Util::scaleSize(4));

    addButton_ = new QToolButton(surface);
    addButton_->setObjectName(QStringLiteral("chatInputGhostButton"));
    addButton_->setText(QStringLiteral("+"));
    addButton_->setCursor(Qt::PointingHandCursor);
    addButton_->setToolTip(QStringLiteral("选择图片引用"));
    connect(addButton_, &QToolButton::clicked, this, &ChatInputWidget::onAddButtonClicked);

    modelButton_ = new QToolButton(surface);
    modelButton_->setObjectName(QStringLiteral("chatInputGhostButton"));
    modelButton_->setCursor(Qt::PointingHandCursor);
    modelButton_->setToolTip(QStringLiteral("切换模型"));
    refreshModelButton();
    connect(modelButton_, &QToolButton::clicked, this, &ChatInputWidget::showModelMenu);

    sendButton_ = new QPushButton(surface);
    sendButton_->setObjectName(QStringLiteral("chatInputSendButton"));
    sendButton_->setFlat(true);
    sendButton_->setText(QString());
    sendButton_->setIcon(chatSendIcon());
    sendButton_->setFixedSize(Util::scaleSize(28), Util::scaleSize(28));
    sendButton_->setIconSize(QSize(Util::scaleSize(kChatInputToolbarIconPx), Util::scaleSize(kChatInputToolbarIconPx)));
    sendButton_->setCursor(Qt::PointingHandCursor);
    connect(sendButton_, &QPushButton::clicked, this, &ChatInputWidget::onSendClicked);

    toolbarLayout->addWidget(addButton_);
    toolbarLayout->addWidget(modelButton_);
    // 联网与浏览器操作互斥：两个图标按钮并排，开启一个会自动关闭另一个。
    webButton_ = new QToolButton(surface);
    webButton_->setObjectName(QStringLiteral("chatInputToggleButton"));
    webButton_->setCheckable(true);
    webButton_->setCursor(Qt::PointingHandCursor);
    webButton_->setIconSize(QSize(Util::scaleSize(kChatInputTogglePx), Util::scaleSize(kChatInputTogglePx)));
    webButton_->setFixedSize(Util::scaleSize(kChatInputTogglePx), Util::scaleSize(kChatInputTogglePx));

    browserButton_ = new QToolButton(surface);
    browserButton_->setObjectName(QStringLiteral("chatInputToggleButton"));
    browserButton_->setCheckable(true);
    browserButton_->setCursor(Qt::PointingHandCursor);
    browserButton_->setIconSize(QSize(Util::scaleSize(kChatInputTogglePx), Util::scaleSize(kChatInputTogglePx)));
    browserButton_->setFixedSize(Util::scaleSize(kChatInputTogglePx), Util::scaleSize(kChatInputTogglePx));

    connect(webButton_, &QToolButton::toggled, this, [this](bool enabled) {
        if (enabled && browserButton_->isChecked()) {
            browserButton_->setChecked(false);  // 与“浏览器”二选一
        }
        refreshToggleButtons();
        emit sigWebEnabledChanged(enabled);
    });
    connect(browserButton_, &QToolButton::toggled, this, [this](bool enabled) {
        if (enabled && webButton_->isChecked()) {
            webButton_->setChecked(false);  // 与“联网”二选一
        }
        refreshToggleButtons();
        emit sigBrowserEnabledChanged(enabled);
    });
    webButton_->setChecked(true);  // 在信号接好之后设置，保证图标配色与互斥状态一致
    refreshToggleButtons();

    toolbarLayout->addWidget(webButton_);
    toolbarLayout->addSpacing(Util::scaleSize(kChatInputToggleGapPx));
    toolbarLayout->addWidget(browserButton_);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(sendButton_);
    surfaceLayout->addLayout(toolbarLayout);

    rootLayout->addWidget(surface);
    adjustInputHeight();
    updateSendButtonState();
}

void ChatInputWidget::setPlaceholderText(const QString &text)
{
    if (inputEdit_) {
        inputEdit_->setPlaceholderText(text);
    }
}

void ChatInputWidget::setPending(bool pending)
{
    pending_ = pending;
    if (webButton_) {
        webButton_->setEnabled(!pending);
    }
    if (browserButton_) {
        browserButton_->setEnabled(!pending);
    }
    updateSendButtonState();
}

bool ChatInputWidget::webEnabled() const
{
    return webButton_ && webButton_->isChecked();
}

void ChatInputWidget::setWebEnabled(bool enabled)
{
    if (webButton_) {
        webButton_->setChecked(enabled);
    }
}

bool ChatInputWidget::browserEnabled() const
{
    return browserButton_ && browserButton_->isChecked();
}

void ChatInputWidget::setBrowserEnabled(bool enabled)
{
    if (browserButton_) {
        browserButton_->setChecked(enabled);
    }
}

void ChatInputWidget::quoteText(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    QuotedReferenceItem item;
    item.type = QuotedReferenceItem::Type::Text;
    item.text = trimmed;
    item.displayName = normalizedPreviewText(trimmed);
    appendQuotedReference(item);
    if (inputEdit_) {
        inputEdit_->setFocus();
    }
    updateSendButtonState();
}

void ChatInputWidget::quoteImage(const QPixmap &image, const QString &displayName)
{
    if (image.isNull()) {
        return;
    }

    QuotedReferenceItem item;
    item.type = QuotedReferenceItem::Type::Image;
    item.displayName = displayName.trimmed().isEmpty()
        ? QStringLiteral("image.png")
        : displayName.trimmed();
    item.image = image;
    appendQuotedReference(item);
    if (inputEdit_) {
        inputEdit_->setFocus();
    }
    updateSendButtonState();
}

void ChatInputWidget::clearQuotedImage()
{
    for (int i = quotedReferences_.size() - 1; i >= 0; --i) {
        if (quotedReferences_.at(i).type == QuotedReferenceItem::Type::Image) {
            quotedReferences_.removeAt(i);
        }
    }
    rebuildQuotedAttachmentsUi();
    updateSendButtonState();
}

bool ChatInputWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == inputEdit_) {
        if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->matches(QKeySequence::Paste)) {
                const QClipboard *clipboard = QApplication::clipboard();
                const QMimeData *mimeData = clipboard ? clipboard->mimeData(QClipboard::Clipboard) : nullptr;
                if (mimeData && mimeData->hasImage()) {
                    const QVariant imageData = mimeData->imageData();
                    if (imageData.canConvert<QImage>()) {
                        quoteImage(QPixmap::fromImage(qvariant_cast<QImage>(imageData)));
                        return true;
                    }
                }
            }

            if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) &&
                !(keyEvent->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
                onSendClicked();
                return true;
            }
        }
    }

    return QWidget::eventFilter(watched, event);
}

void ChatInputWidget::onAddButtonClicked()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择图片"),
        QString(),
        QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.webp *.gif)"));
    if (filePath.isEmpty()) {
        return;
    }

    QPixmap image(filePath);
    if (image.isNull()) {
        return;
    }

    quoteImage(image, QFileInfo(filePath).fileName());
}

void ChatInputWidget::onSendClicked()
{
    if (!inputEdit_) {
        return;
    }

    const QString inputText = trimmedInputText();
    const bool hasPayload = !inputText.isEmpty() || !quotedReferences_.isEmpty();

    if (pending_) {
        if (!hasPayload) {
            emit sigStopRequested();
            return;
        }
    } else if (!hasPayload) {
        return;
    }

    QStringList textParts;
    QList<QPixmap> images;
    images.reserve(quotedReferences_.size());

    for (const QuotedReferenceItem &item : quotedReferences_) {
        if (item.type == QuotedReferenceItem::Type::Text) {
            if (!item.text.trimmed().isEmpty()) {
                textParts.append(item.text.trimmed());
            }
            continue;
        }

        if (!item.image.isNull()) {
            images.append(item.image);
        }
    }

    if (!inputText.isEmpty()) {
        textParts.append(inputText);
    }

    inputEdit_->clear();
    clearQuotedReferences();
    updateSendButtonState();
    emit sigSendRequested(textParts.join(QStringLiteral("\n\n")), images);
}

void ChatInputWidget::showModelMenu()
{
    auto *setting = settings_;
    const QList<LlmProviderConfig> providers = setting->llmProviders();
    if (providers.isEmpty()) {
        return;
    }

    const int activeIndex = setting->llmActiveProviderIndex();

    QMenu menu(this);
    menu.setObjectName(QStringLiteral("chatModelMenu"));
    for (int i = 0; i < providers.size(); ++i) {
        const LlmProviderConfig &p = providers.at(i);
        QString label = p.name.isEmpty() ? p.model : p.name;
        if (!p.model.isEmpty() && !p.name.isEmpty() && p.name != p.model) {
            label += QStringLiteral("  (%1)").arg(p.model);
        }
        if (label.isEmpty()) {
            label = QStringLiteral("Provider %1").arg(i + 1);
        }

        QAction *action = menu.addAction(label);
        action->setCheckable(true);
        action->setChecked(i == activeIndex);

        connect(action, &QAction::triggered, this, [this, i]() {
            settings_->setLlmActiveProviderIndex(i);
            refreshModelButton();
        });
    }

    menu.exec(modelButton_->mapToGlobal(QPoint(0, modelButton_->height())));
}

void ChatInputWidget::appendQuotedReference(const QuotedReferenceItem &item)
{
    if ((item.type == QuotedReferenceItem::Type::Text && item.text.trimmed().isEmpty()) ||
        (item.type == QuotedReferenceItem::Type::Image && item.image.isNull())) {
        return;
    }

    if (quotedReferences_.size() >= kMaxQuotedReferences) {
        quotedReferences_.removeFirst();
    }

    quotedReferences_.append(item);
    rebuildQuotedAttachmentsUi();
}

void ChatInputWidget::previewQuotedReference(int index)
{
    if (index < 0 || index >= quotedReferences_.size()) {
        return;
    }

    const QuotedReferenceItem &item = quotedReferences_.at(index);
    if (item.type == QuotedReferenceItem::Type::Image) {
        showImagePreviewDialog(this, item.image, item.displayName);
        return;
    }

    showTextPreviewDialog(this, item.text);
}

void ChatInputWidget::updateSendButtonState()
{
    if (!sendButton_ || !inputEdit_) {
        return;
    }

    if (pending_) {
        sendButton_->setEnabled(true);
        const bool hasPayload = !trimmedInputText().isEmpty() || !quotedReferences_.isEmpty();
        if (hasPayload) {
            sendButton_->setIcon(chatSendIcon());
            sendButton_->setToolTip(QStringLiteral("加入待发送队列"));
        } else {
            sendButton_->setIcon(chatStopIcon());
            sendButton_->setToolTip(QStringLiteral("中断生成"));
        }
        return;
    }

    sendButton_->setIcon(chatSendIcon());
    sendButton_->setToolTip(QStringLiteral("发送"));
    sendButton_->setEnabled(
        !trimmedInputText().isEmpty() ||
        !quotedReferences_.isEmpty());
}

void ChatInputWidget::refreshToggleButtons()
{
    if (webButton_) {
        const bool enabled = webButton_->isChecked();
        webButton_->setIcon(chatToggleIcon(QStringLiteral("web.png"), enabled));
        webButton_->setToolTip(enabled
            ? QStringLiteral("联网已开启：模型可搜索、读取网页（点击关闭）")
            : QStringLiteral("联网已关闭：点击允许模型搜索、读取网页（与浏览器二选一）"));
    }
    if (browserButton_) {
        const bool enabled = browserButton_->isChecked();
        browserButton_->setIcon(chatToggleIcon(QStringLiteral("chrome.png"), enabled));
        browserButton_->setToolTip(enabled
            ? QStringLiteral("浏览器已开启：模型可操作右侧浏览器（点击关闭）")
            : QStringLiteral("浏览器已关闭：点击允许模型操作右侧浏览器（与联网二选一）"));
    }
}

void ChatInputWidget::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange) {
        // 图标按主题重着色，换肤后要重新取一遍。
        refreshToggleButtons();
        updateSendButtonState();
    }
}

void ChatInputWidget::refreshModelButton()
{
    if (!modelButton_) {
        return;
    }

    const LlmProviderConfig active = settings_->llmActiveProvider();
    const QString display = active.name.isEmpty() ? active.model : active.name;
    modelButton_->setText(display.isEmpty() ? QStringLiteral("模型") : display);
}

void ChatInputWidget::rebuildQuotedAttachmentsUi()
{
    if (!quoteContainer_ || !quoteLayout_) {
        return;
    }

    while (quoteLayout_->count() > 0) {
        QLayoutItem *item = quoteLayout_->takeAt(0);
        if (QWidget *widget = item->widget()) {
            delete widget;
        }
        delete item;
    }

    for (int i = 0; i < quotedReferences_.size(); ++i) {
        const QuotedReferenceItem &item = quotedReferences_.at(i);
        QFrame *chip = new QFrame(quoteContainer_);
        chip->setObjectName(QStringLiteral("chatQuoteChip"));
        chip->setFixedHeight(Util::scaleSize(26));
        chip->setMinimumWidth(Util::scaleSize(96));
        chip->setMaximumWidth(Util::scaleSize(220));
        chip->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        auto *chipLayout = new QHBoxLayout(chip);
        chipLayout->setContentsMargins(Util::scaleSize(5), Util::scaleSize(2), Util::scaleSize(3), Util::scaleSize(2));
        chipLayout->setSpacing(Util::scaleSize(2));

        ElidedPushButton *previewButton = new ElidedPushButton(chip);
        previewButton->setObjectName(QStringLiteral("chatQuotePreviewButton"));
        previewButton->setCursor(Qt::PointingHandCursor);
        previewButton->setFocusPolicy(Qt::NoFocus);
        previewButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        previewButton->setToolTip(item.type == QuotedReferenceItem::Type::Image
            ? QStringLiteral("%1\n点击预览").arg(item.displayName)
            : QStringLiteral("%1\n\n点击预览").arg(item.text));
        previewButton->setIcon(item.type == QuotedReferenceItem::Type::Image
            ? createQuotedImageThumbnail(item.image)
            : QIcon());
        previewButton->setIconSize(QSize(Util::scaleSize(18), Util::scaleSize(18)));
        previewButton->setFullText(item.type == QuotedReferenceItem::Type::Image
            ? item.displayName
            : normalizedPreviewText(item.text));
        connect(previewButton, &QPushButton::clicked, this, [this, i]() {
            previewQuotedReference(i);
        });

        QToolButton *removeButton = new QToolButton(chip);
        removeButton->setObjectName(QStringLiteral("chatQuoteRemoveButton"));
        removeButton->setText(QStringLiteral("×"));
        removeButton->setCursor(Qt::PointingHandCursor);
        removeButton->setFocusPolicy(Qt::NoFocus);
        removeButton->setToolTip(QStringLiteral("移除引用"));
        connect(removeButton, &QToolButton::clicked, this, [this, i]() {
            if (i >= 0 && i < quotedReferences_.size()) {
                quotedReferences_.removeAt(i);
                rebuildQuotedAttachmentsUi();
                updateSendButtonState();
                if (inputEdit_) {
                    QTimer::singleShot(0, this, [this]() {
                        if (inputEdit_) {
                            inputEdit_->setFocus(Qt::MouseFocusReason);
                        }
                    });
                }
            }
        });

        chipLayout->addWidget(previewButton, 1);
        chipLayout->addWidget(removeButton, 0);
        quoteLayout_->addWidget(chip, 0, Qt::AlignLeft | Qt::AlignVCenter);
    }

    quoteLayout_->addStretch(1);
    quoteContainer_->setVisible(!quotedReferences_.isEmpty());
}

void ChatInputWidget::clearQuotedReferences()
{
    quotedReferences_.clear();
    rebuildQuotedAttachmentsUi();
}

void ChatInputWidget::adjustInputHeight()
{
    if (!inputEdit_) {
        return;
    }

    const QFontMetrics fm(inputEdit_->font());
    const QMargins margins = inputEdit_->contentsMargins();
    const int documentMargin = qRound(inputEdit_->document()->documentMargin());
    const int frameWidth = inputEdit_->frameWidth();
    const int lineHeight = fm.lineSpacing();
    const int blockCount = qMax(1, inputEdit_->document()->blockCount());
    const int visibleLines = qBound(kChatInputMinVisibleLines, blockCount, kChatInputMaxVisibleLines);
    const int verticalInsets = margins.top() + margins.bottom() + documentMargin * 2 + frameWidth * 2 + Util::scaleSize(8);
    const int targetHeight = qMax(Util::scaleSize(28), visibleLines * lineHeight + verticalInsets);

    inputEdit_->setFixedHeight(targetHeight);
    inputEdit_->setVerticalScrollBarPolicy(blockCount > kChatInputMaxVisibleLines
        ? Qt::ScrollBarAsNeeded
        : Qt::ScrollBarAlwaysOff);
}

QString ChatInputWidget::trimmedInputText() const
{
    return inputEdit_ ? inputEdit_->toPlainText().trimmed() : QString();
}
