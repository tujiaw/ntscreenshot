#include "modules/local_search/ui/LocalSearchWidget.h"

#include "core/platform/Util.h"
#include "core/theme/ThemeIcon.h"
#include "core/theme/ThemeManager.h"
#include "core/theme/UiStyler.h"
#include "modules/local_search/application/SearchIndexService.h"
#include "modules/local_search/application/SearchProviders.h"
#include "modules/local_search/infrastructure/SearchIndexStore.h"
#include "core/settings/SettingModel.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFrame>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPaintEvent>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QProcess>
#include <QRegularExpression>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSaveFile>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrent>
#include <QStyle>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#endif

namespace {

constexpr QChar kWebSearchPrefix = u'<';
constexpr qint64 kFaviconCacheMaxAgeSeconds = 24 * 60 * 60;
constexpr int kShadowPad = 12;
constexpr int kShadowOffY = 6;
constexpr int kInnerPad = 8;
constexpr int kQueryHeight = 40;
constexpr int kCornerRadius = 12;

enum SearchItemRole {
    NameRole = Qt::UserRole + 10,
    PathRole,
    MetadataRole
};

class SearchGlyph final : public QWidget
{
public:
    explicit SearchGlyph(QWidget* parent = nullptr) : QWidget(parent)
    {
        setFixedSize(24, 24);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        QPen pen(ThemeManager::tokens().textSecondary, 1.8);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QRectF(5.0, 5.0, 14.0, 14.0));
        painter.drawLine(QPointF(16.0, 16.0), QPointF(19.0, 19.0));
    }
};

class SearchResultDelegate final : public QStyledItemDelegate
{
public:
    explicit SearchResultDelegate(qreal scale, QObject* parent = nullptr)
        : QStyledItemDelegate(parent), scale_(scale) {}

    void setScale(qreal scale) { scale_ = scale; }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const ThemeTokens& tokens = ThemeManager::tokens();
        const bool selected = option.state.testFlag(QStyle::State_Selected);
        const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
        const QRect row = option.rect.adjusted(px(10), px(1), -px(8), -px(1));

        if (selected || hovered) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(selected ? tokens.accentSubtle : tokens.surfaceSubtle);
            painter->drawRoundedRect(option.rect.adjusted(0, px(1), 0, -px(1)),
                                     px(7), px(7));
        }
        if (selected) {
            painter->setBrush(tokens.accent);
            painter->drawRoundedRect(QRect(option.rect.left(),
                                           option.rect.top() + px(7), px(3),
                                           option.rect.height() - px(14)),
                                     px(1.5), px(1.5));
        }

        const QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        const int iconSize = px(28);
        const QRect iconRect(row.left() + px(6), row.center().y() - iconSize / 2,
                             iconSize, iconSize);
        icon.paint(painter, iconRect, Qt::AlignCenter, QIcon::Normal,
                   selected ? QIcon::On : QIcon::Off);

        const int textLeft = iconRect.right() + px(10);
        const int rightPadding = px(10);
        const QString name = index.data(NameRole).toString().isEmpty()
            ? index.data(Qt::DisplayRole).toString().section(u'\n', 0, 0)
            : index.data(NameRole).toString();
        const QString path = index.data(PathRole).toString().isEmpty()
            ? index.data(Qt::DisplayRole).toString().section(u'\n', 1)
            : index.data(PathRole).toString();
        const QString metadata = index.data(MetadataRole).toString();

        QFont nameFont = option.font;
        nameFont.setPixelSize(px(13));
        nameFont.setWeight(QFont::DemiBold);
        painter->setFont(nameFont);
        painter->setPen(tokens.textPrimary);
        const QRect nameRect(textLeft, row.top() + px(4),
                             row.right() - textLeft - rightPadding, px(17));
        painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
                          QFontMetrics(nameFont).elidedText(name, Qt::ElideRight,
                                                           nameRect.width()));

        QFont detailFont = option.font;
        detailFont.setPixelSize(px(12));
        painter->setFont(detailFont);
        painter->setPen(tokens.textSecondary);
        const QFontMetrics detailMetrics(detailFont);
        const int metaWidth = metadata.isEmpty() ? 0 : detailMetrics.horizontalAdvance(metadata);
        const QRect detailRect(textLeft, row.top() + px(23),
                               qMax(px(80), row.right() - textLeft - rightPadding
                                                    - metaWidth - px(10)), px(16));
        painter->drawText(detailRect, Qt::AlignLeft | Qt::AlignVCenter,
                          detailMetrics.elidedText(path, Qt::ElideMiddle, detailRect.width()));
        if (!metadata.isEmpty()) {
            painter->drawText(QRect(row.right() - rightPadding - metaWidth,
                                    detailRect.top(), metaWidth, detailRect.height()),
                              Qt::AlignRight | Qt::AlignVCenter, metadata);
        }
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override
    {
        return QSize(0, px(46));
    }

private:
    int px(int value) const { return qRound(value * scale_); }
    qreal scale_ = 1.0;
};

enum ContextAction {
    ActionOpen = 0,
    ActionLocate,
    ActionCopyPath,
    ActionRunAsAdmin,
    ActionOpenTerminal,
    ActionOpenEngineHome,
    ActionCopyEngineUrl,
    ActionSetDefaultEngine
};

bool canRunAsAdmin(const SearchResult& result)
{
    const QString target = result.launchTarget.isEmpty() ? result.path : result.launchTarget;
    if (target.isEmpty() || target.startsWith(QStringLiteral("shell:"), Qt::CaseInsensitive)) {
        return false;
    }
    const QFileInfo info(target);
    if (!info.exists() || !info.isFile()) return false;
    const QString ext = info.suffix().toLower();
    return result.type == SearchItemType::Application
        || ext == QStringLiteral("exe")
        || ext == QStringLiteral("bat")
        || ext == QStringLiteral("cmd")
        || ext == QStringLiteral("com")
        || ext == QStringLiteral("msi");
}

QString terminalDirectoryFor(const SearchResult& result)
{
    if (result.type == SearchItemType::Directory) return result.path;
    if (!result.parentPath.isEmpty()) return result.parentPath;
    return QFileInfo(result.path).absolutePath();
}

QIcon contextActionIcon(const QString &resourceName)
{
    return ThemeIcon::icon(resourceName, IconTone::Muted, 28);
}

QString htmlAttribute(const QString& tag, const QString& name)
{
    const QRegularExpression expression(
        QStringLiteral("(?:^|\\s)%1\\s*=\\s*(?:\"([^\"]*)\"|'([^']*)'|([^\\s>]+))")
            .arg(QRegularExpression::escape(name)),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = expression.match(tag);
    if (!match.hasMatch()) return {};
    for (int capture = 1; capture <= 3; ++capture) {
        if (match.hasCaptured(capture)) return match.captured(capture).trimmed();
    }
    return {};
}

QString decodeCommonHtmlEntities(QString value)
{
    value.replace(QStringLiteral("&amp;"), QStringLiteral("&"), Qt::CaseInsensitive);
    value.replace(QStringLiteral("&quot;"), QStringLiteral("\""), Qt::CaseInsensitive);
    value.replace(QStringLiteral("&apos;"), QStringLiteral("'"), Qt::CaseInsensitive);
    value.replace(QStringLiteral("&#39;"), QStringLiteral("'"), Qt::CaseInsensitive);
    return value;
}

int faviconCandidateScore(const QStringList& relValues, const QString& sizes,
                          const QString& type)
{
    int score = 0;
    if (relValues.contains(QStringLiteral("icon"))) {
        score = 1000;
    } else if (relValues.contains(QStringLiteral("apple-touch-icon"))
               || relValues.contains(QStringLiteral("apple-touch-icon-precomposed"))) {
        score = 600;
    } else if (relValues.contains(QStringLiteral("mask-icon"))) {
        score = 300;
    } else {
        return -1;
    }

    const QString normalizedType = type.toLower();
    if (normalizedType == QStringLiteral("image/png")
        || normalizedType == QStringLiteral("image/x-icon")
        || normalizedType == QStringLiteral("image/vnd.microsoft.icon")) {
        score += 30;
    } else if (normalizedType == QStringLiteral("image/svg+xml")) {
        score += 20;
    }

    if (sizes.compare(QStringLiteral("any"), Qt::CaseInsensitive) == 0) {
        return score + 80;
    }
    const QRegularExpression sizeExpression(QStringLiteral("(\\d+)\\s*x\\s*(\\d+)"),
                                            QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator matches = sizeExpression.globalMatch(sizes);
    int bestSizeScore = sizes.isEmpty() ? 50 : 0;
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        const int width = match.captured(1).toInt();
        const int height = match.captured(2).toInt();
        if (width <= 0 || height <= 0) continue;
        const int distance = qAbs(qMin(width, height) - 32);
        bestSizeScore = qMax(bestSizeScore, qMax(0, 100 - distance));
    }
    return score + bestSizeScore;
}

QUrl declaredFaviconUrl(const QByteArray& pageData, const QUrl& pageUrl)
{
    const QString html = QString::fromUtf8(pageData);
    QUrl documentBaseUrl = pageUrl;
    const QRegularExpression baseExpression(
        QStringLiteral("<base\\b[^>]*>"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch baseMatch = baseExpression.match(html);
    if (baseMatch.hasMatch()) {
        const QString baseHref = decodeCommonHtmlEntities(
            htmlAttribute(baseMatch.captured(), QStringLiteral("href")));
        const QUrl resolvedBase = pageUrl.resolved(QUrl::fromEncoded(baseHref.toUtf8()));
        if (resolvedBase.isValid()) documentBaseUrl = resolvedBase;
    }

    const QRegularExpression linkExpression(
        QStringLiteral("<link\\b[^>]*>"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator links = linkExpression.globalMatch(html);
    QUrl bestUrl;
    int bestScore = -1;
    while (links.hasNext()) {
        const QString tag = links.next().captured();
        const QStringList relValues = htmlAttribute(tag, QStringLiteral("rel"))
                                          .toLower()
                                          .split(QRegularExpression(QStringLiteral("\\s+")),
                                                 Qt::SkipEmptyParts);
        const int score = faviconCandidateScore(
            relValues,
            htmlAttribute(tag, QStringLiteral("sizes")),
            htmlAttribute(tag, QStringLiteral("type")));
        if (score < 0 || score <= bestScore) continue;

        const QString href = decodeCommonHtmlEntities(
            htmlAttribute(tag, QStringLiteral("href")));
        if (href.isEmpty()) continue;
        const QUrl resolved = documentBaseUrl.resolved(QUrl::fromEncoded(href.toUtf8()));
        if (resolved.scheme() == QStringLiteral("http")
            || resolved.scheme() == QStringLiteral("https")) {
            bestUrl = resolved;
            bestScore = score;
        }
    }
    return bestUrl;
}

QString bookmarkGroupLabel(const QString& parentPath)
{
    QStringList parts = parentPath.split(QStringLiteral(" · "), Qt::SkipEmptyParts);
    if (parts.isEmpty()) return {};

    QStringList labelParts{parts.takeFirst().trimmed()};
    QStringList folders = parts.join(QStringLiteral(" / "))
                              .split(QStringLiteral(" / "), Qt::SkipEmptyParts);
    if (!folders.isEmpty()) {
        const QString root = folders.first().trimmed();
        if (root.compare(QStringLiteral("书签栏"), Qt::CaseInsensitive) == 0
            || root.compare(QStringLiteral("收藏夹栏"), Qt::CaseInsensitive) == 0
            || root.compare(QStringLiteral("Bookmarks bar"), Qt::CaseInsensitive) == 0
            || root.compare(QStringLiteral("Favorites bar"), Qt::CaseInsensitive) == 0) {
            folders.removeFirst();
        }
    }
    for (const QString& folder : folders) {
        const QString trimmed = folder.trimmed();
        if (!trimmed.isEmpty()) labelParts.push_back(trimmed);
    }
    return labelParts.join(u'/');
}

QIcon webPageFallbackIcon()
{
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(QApplication::palette().color(QPalette::Link), 2.0);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    const QRectF globe(4.5, 4.5, 23.0, 23.0);
    painter.drawEllipse(globe);
    painter.drawEllipse(QRectF(10.5, 4.5, 11.0, 23.0));
    painter.drawLine(QPointF(5.5, 12.0), QPointF(26.5, 12.0));
    painter.drawLine(QPointF(5.5, 20.0), QPointF(26.5, 20.0));
    return QIcon(pixmap);
}

QIcon searchEngineFallbackIcon(const QString& engineId, int size)
{
    const int pixelSize = qMax(16, size);
    QPixmap pixmap(pixelSize, pixelSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor bg;
    QString letter;
    if (engineId == QStringLiteral("google")) {
        bg = QColor(0x42, 0x85, 0xF4);
        letter = QStringLiteral("G");
    } else if (engineId == QStringLiteral("bing")) {
        bg = QColor(0x00, 0x80, 0x9D);
        letter = QStringLiteral("B");
    } else if (engineId == QStringLiteral("baidu")) {
        bg = QColor(0x29, 0x3A, 0xED);
        letter = QStringLiteral("百");
    } else if (engineId == QStringLiteral("duckduckgo")) {
        bg = QColor(0xDE, 0x58, 0x33);
        letter = QStringLiteral("D");
    } else if (engineId == QStringLiteral("perplexity")) {
        bg = QColor(0x20, 0x80, 0x8D);
        letter = QStringLiteral("P");
    } else if (engineId == QStringLiteral("metaso")) {
        bg = QColor(0x10, 0xB9, 0x81);
        letter = QStringLiteral("秘");
    } else if (engineId == QStringLiteral("tiangong")) {
        bg = QColor(0x25, 0x63, 0xEB);
        letter = QStringLiteral("天");
    } else {
        bg = QApplication::palette().color(QPalette::Mid);
        letter = QStringLiteral("?");
    }

    const qreal center = pixelSize / 2.0;
    const qreal radius = pixelSize * 0.44;
    painter.setPen(Qt::NoPen);
    painter.setBrush(bg);
    painter.drawEllipse(QPointF(center, center), radius, radius);

    QFont font;
    const bool isCjk = letter.length() == 1 && letter.at(0).unicode() > 0x2E80;
    font.setPixelSize(qMax(10, qRound(pixelSize * (isCjk ? 0.42 : 0.48))));
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QRect(0, 0, pixelSize, pixelSize), Qt::AlignCenter, letter);

    return QIcon(pixmap);
}

QIcon searchEngineIcon(const WebSearchEngine& engine, int size)
{
    const int pixelSize = qMax(16, size);
    if (!engine.iconPath.isEmpty()) {
        const QIcon raw(engine.iconPath);
        QPixmap source = raw.pixmap(pixelSize, pixelSize);
        if (!source.isNull()) {
            if (source.width() != pixelSize || source.height() != pixelSize) {
                source = source.scaled(pixelSize, pixelSize, Qt::KeepAspectRatio,
                                       Qt::SmoothTransformation);
                QPixmap canvas(pixelSize, pixelSize);
                canvas.fill(Qt::transparent);
                QPainter painter(&canvas);
                painter.setRenderHint(QPainter::SmoothPixmapTransform);
                painter.drawPixmap((pixelSize - source.width()) / 2,
                                   (pixelSize - source.height()) / 2, source);
                source = canvas;
            }
            return QIcon(source);
        }
    }
    return searchEngineFallbackIcon(engine.id, pixelSize);
}

bool isWebSearchMode(const QString& text)
{
    return text.trimmed().startsWith(kWebSearchPrefix);
}

bool haveSameDisplayedResults(const QVector<SearchResult>& left,
                              const QVector<SearchResult>& right)
{
    if (left.size() != right.size()) return false;
    for (qsizetype index = 0; index < left.size(); ++index) {
        const SearchResult& leftResult = left[index];
        const SearchResult& rightResult = right[index];
        if (leftResult.type != rightResult.type
            || leftResult.name != rightResult.name
            || leftResult.path != rightResult.path
            || leftResult.parentPath != rightResult.parentPath
            || leftResult.launchTarget != rightResult.launchTarget) {
            return false;
        }
    }
    return true;
}

QString webSearchTerms(const QString& text)
{
    const QString trimmed = text.trimmed();
    return trimmed.startsWith(kWebSearchPrefix) ? trimmed.mid(1).trimmed() : QString();
}

#ifdef Q_OS_WIN
QIcon windowsShellIcon(const QString& target)
{
    if (target.isEmpty()) return {};

    PIDLIST_ABSOLUTE itemId = nullptr;
    const HRESULT parsed = SHParseDisplayName(
        reinterpret_cast<LPCWSTR>(target.utf16()), nullptr, &itemId, 0, nullptr);
    if (FAILED(parsed) || !itemId) return {};

    SHFILEINFOW shellInfo{};
    const DWORD_PTR result = SHGetFileInfoW(
        reinterpret_cast<LPCWSTR>(itemId), 0, &shellInfo, sizeof(shellInfo),
        SHGFI_PIDL | SHGFI_ICON | SHGFI_LARGEICON);
    CoTaskMemFree(itemId);
    if (result == 0 || !shellInfo.hIcon) return {};

    const QIcon icon(QPixmap::fromImage(QImage::fromHICON(shellInfo.hIcon)));
    DestroyIcon(shellInfo.hIcon);
    return icon;
}
#endif

} // namespace

LocalSearchWidget::LocalSearchWidget(std::shared_ptr<SearchAggregator> aggregator,
                                     std::shared_ptr<SearchIndexStore> store,
                                     SearchIndexService* service,
                                     QWidget* parent)
    : QWidget(parent)
    , aggregator_(std::move(aggregator))
    , store_(std::move(store))
    , service_(service)
    , searchWatcher_(new QFutureWatcher<QVector<SearchResult>>(this))
    , searchTimer_(new QTimer(this))
    , network_(new QNetworkAccessManager(this))
    , scaleFactor_(Util::getScreenScaleFactor())
{
    faviconCacheDirectory_ = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation) + QStringLiteral("/favicons");
    QDir().mkpath(faviconCacheDirectory_);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setObjectName(QStringLiteral("LocalSearchLauncher"));
    setWindowTitle(QStringLiteral("本地搜索"));
    rootLayout_ = new QVBoxLayout(this);
    rootLayout_->setContentsMargins(scaled(kShadowPad + kInnerPad),
                                    scaled(kShadowPad + kInnerPad),
                                    scaled(kShadowPad + kInnerPad),
                                    scaled(kShadowPad + kInnerPad + kShadowOffY));
    rootLayout_->setSpacing(0);
    rootLayout_->setSizeConstraint(QLayout::SetNoConstraint);
    requestedWidth_ = scaled(580);
    setFixedWidth(requestedWidth_ + scaled(kShadowPad) * 2);
    setMinimumHeight(compactHeight());

    query_ = new QLineEdit(this);
    query_->setPlaceholderText(QStringLiteral("搜索文件、目录、应用或书签…"));
    query_->setClearButtonEnabled(false);
    query_->setFixedHeight(scaled(kQueryHeight));
    query_->setFrame(false);
    QFont queryFont = query_->font();
    queryFont.setPixelSize(scaled(15));
    query_->setFont(queryFont);
    query_->setTextMargins(scaled(2), 0, scaled(70), 0);
    UiStyler::setRole(query_, UiRole::SearchField);
    query_->installEventFilter(this);

    resultCount_ = new QLabel(this);
    resultCount_->setObjectName(QStringLiteral("localSearchCount"));
    resultCount_->setContentsMargins(0, 0, scaled(12), 0);
    resultCount_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    resultCount_->setTextInteractionFlags(Qt::NoTextInteraction);
    resultCount_->setAttribute(Qt::WA_TransparentForMouseEvents);

    // 输入行：图标 + 输入框(铺满)。"找到 N 项"计数标签与输入框同一网格单元、
    // 右对齐浮在搜索框最右侧，不参与宽度布局，输入框宽度恒定、不被挤压。
    auto* searchRow = new QGridLayout();
    searchRow->setContentsMargins(0, 0, 0, 0);
    searchRow->setHorizontalSpacing(0);
    searchRow->setVerticalSpacing(0);
    auto* searchGlyph = new SearchGlyph(this);
    searchRow->addWidget(searchGlyph, 0, 0, Qt::AlignVCenter);
    searchRow->addWidget(query_, 0, 1);
    searchRow->setColumnStretch(1, 1);
    searchRow->addWidget(resultCount_, 0, 1, Qt::AlignRight | Qt::AlignVCenter);
    rootLayout_->addLayout(searchRow);

    resultsPanel_ = new QWidget(this);
    resultsLayout_ = new QVBoxLayout(resultsPanel_);
    resultsLayout_->setContentsMargins(0, scaled(6), 0, 0);
    resultsLayout_->setSpacing(0);

    stackedWidget_ = new QStackedWidget(resultsPanel_);

    results_ = new QListWidget();
    results_->setObjectName(QStringLiteral("localSearchResults"));
    results_->setAlternatingRowColors(false);
    results_->setUniformItemSizes(true);
    results_->setIconSize(QSize(scaled(32), scaled(32)));
    results_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    results_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    results_->setFrameShape(QFrame::NoFrame);
    results_->setMouseTracking(true);
    results_->setItemDelegate(new SearchResultDelegate(scaleFactor_, results_));
    results_->installEventFilter(this);
    stackedWidget_->addWidget(results_);

    contextMenuList_ = new QListWidget();
    contextMenuList_->setObjectName(QStringLiteral("localSearchContextMenu"));
    contextMenuList_->setAlternatingRowColors(false);
    contextMenuList_->setUniformItemSizes(true);
    contextMenuList_->setIconSize(QSize(scaled(28), scaled(28)));
    contextMenuList_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    contextMenuList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    contextMenuList_->setFrameShape(QFrame::NoFrame);
    contextMenuList_->installEventFilter(this);
    stackedWidget_->addWidget(contextMenuList_);

    stackedWidget_->setCurrentIndex(0);
    resultsLayout_->addWidget(stackedWidget_, 1);

    auto* footerDivider = new QFrame(resultsPanel_);
    footerDivider->setObjectName(QStringLiteral("localSearchDivider"));
    footerDivider->setFixedHeight(qMax(1, scaled(1)));
    resultsLayout_->addWidget(footerDivider);
    footerHint_ = new QLabel(
        QStringLiteral("↑↓ 选择    Enter 打开    Shift+Enter 更多    < 网络搜索"),
        resultsPanel_);
    footerHint_->setObjectName(QStringLiteral("localSearchFooter"));
    footerHint_->setFixedHeight(scaled(28));
    footerHint_->setContentsMargins(scaled(12), 0, scaled(12), 0);
    resultsLayout_->addWidget(footerHint_);
    rootLayout_->addWidget(resultsPanel_, 1);
    resultsPanel_->hide();

    searchTimer_->setSingleShot(true);
    searchTimer_->setInterval(70);
    connect(query_, &QLineEdit::textChanged, this, [this]() { searchTimer_->start(); });
    connect(searchTimer_, &QTimer::timeout, this, &LocalSearchWidget::startSearch);
    connect(query_, &QLineEdit::returnPressed, this, &LocalSearchWidget::openCurrent);
    connect(results_, &QListWidget::itemActivated, this, [this]() { openCurrent(); });
    connect(contextMenuList_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (item) executeContextAction(item->data(Qt::UserRole).toInt());
    });
    connect(service_, &SearchIndexService::indexUpdated, this, [this]() {
        if (!query_->text().trimmed().isEmpty()) startSearch();
    });
    auto* escapeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escapeShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escapeShortcut, &QShortcut::activated, this, [this]() {
        if (contextMenuVisible_) {
            hideContextMenu();
        } else {
            emit requestHide();
        }
    });
}

void LocalSearchWidget::activateSearch()
{
    searchTimer_->stop();
    pendingQuery_.clear();
    rerunPending_ = false;
    showingEngines_ = false;
    currentResults_.clear();
    results_->clear();
    if (contextMenuVisible_) {
        contextMenuVisible_ = false;
        contextMenuList_->clear();
        stackedWidget_->setCurrentIndex(0);
    }
    {
        const QSignalBlocker blocker(query_);
        query_->clear();
    }
    resultsVisible_ = false;
    resultsPanel_->hide();
    resultCount_->clear();
    rootLayout_->invalidate();
    rootLayout_->activate();
    updateGeometry();
    query_->setFocus(Qt::ShortcutFocusReason);
}

void LocalSearchWidget::setScaleFactor(qreal scaleFactor)
{
    const qreal normalized = qMax<qreal>(1.0, scaleFactor);
    if (qFuzzyCompare(scaleFactor_, normalized)) return;
    scaleFactor_ = normalized;

    rootLayout_->setContentsMargins(scaled(kShadowPad + kInnerPad),
                                    scaled(kShadowPad + kInnerPad),
                                    scaled(kShadowPad + kInnerPad),
                                    scaled(kShadowPad + kInnerPad + kShadowOffY));
    query_->setFixedHeight(scaled(kQueryHeight));
    QFont queryFont = query_->font();
    queryFont.setPixelSize(scaled(15));
    query_->setFont(queryFont);
    query_->setTextMargins(scaled(2), 0, scaled(70), 0);
    resultsLayout_->setContentsMargins(0, scaled(6), 0, 0);
    results_->setIconSize(QSize(scaled(32), scaled(32)));
    contextMenuList_->setIconSize(QSize(scaled(28), scaled(28)));
    if (auto* delegate = dynamic_cast<SearchResultDelegate*>(results_->itemDelegate())) {
        delegate->setScale(scaleFactor_);
    }
    if (footerHint_) footerHint_->setFixedHeight(scaled(28));
    setAvailableWidth(availableWidth_);
    setMinimumHeight(compactHeight());
    rootLayout_->invalidate();
    rootLayout_->activate();
}

void LocalSearchWidget::setAvailableWidth(int width)
{
    availableWidth_ = width;
    const int preferred = width > 0 ? qRound(width * 0.35) : scaled(580);
    requestedWidth_ = qBound(scaled(500), preferred, scaled(600));
    setFixedWidth(requestedWidth_ + scaled(kShadowPad) * 2);
}

void LocalSearchWidget::setWebSearchEngine(const QString& baseUrl, const QString& queryParam)
{
    webSearchUrl_ = baseUrl;
    webSearchQueryParam_ = queryParam;
}

void LocalSearchWidget::setPinyinSearchEnabled(bool enabled)
{
    pinyinEnabled_ = enabled;
}

QSize LocalSearchWidget::compactWindowSize() const
{
    // 使用请求的目标宽度而不是 width()：布局尚未运行时 width() 可能还是旧值，
    // 会导致窗口在结果列表出现/消失时几何跳动、输入框位置偏移。
    // requestedWidth_ 为卡片可视宽度，窗口总宽额外包含两侧阴影留白。
    const int cardWidth = requestedWidth_ > 0 ? requestedWidth_ : width() - scaled(kShadowPad) * 2;
    return QSize(cardWidth + scaled(kShadowPad) * 2, compactHeight());
}

void LocalSearchWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        if (contextMenuVisible_) {
            hideContextMenu();
        } else {
            emit requestHide();
        }
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

bool LocalSearchWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        const Qt::KeyboardModifiers modifiers = keyEvent->modifiers();

        const bool isEnter = keyEvent->key() == Qt::Key_Return
                             || keyEvent->key() == Qt::Key_Enter;
        const bool isEscape = keyEvent->key() == Qt::Key_Escape;

        const bool shiftOnly = modifiers.testFlag(Qt::ShiftModifier)
            && !(modifiers & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
        const bool shiftEnter = isEnter && shiftOnly;

        // Context menu list: Enter triggers action, Shift+Enter / Escape go back
        if (watched == contextMenuList_) {
            if (shiftEnter) {
                hideContextMenu();
                return true;
            }
            if (isEnter && !shiftOnly) {
                QListWidgetItem* item = contextMenuList_->currentItem();
                if (item) executeContextAction(item->data(Qt::UserRole).toInt());
                return true;
            }
            if (isEscape) {
                hideContextMenu();
                return true;
            }
            const bool shiftNav = shiftOnly && !(modifiers
                & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
            const bool arrowNav = !(modifiers & (Qt::ShiftModifier | Qt::ControlModifier
                                                  | Qt::AltModifier | Qt::MetaModifier));
            const bool ctxMoveDown = (shiftNav && keyEvent->key() == Qt::Key_N)
                                      || (arrowNav && keyEvent->key() == Qt::Key_Down);
            const bool ctxMoveUp = (shiftNav && keyEvent->key() == Qt::Key_P)
                                    || (arrowNav && keyEvent->key() == Qt::Key_Up);
            if (ctxMoveDown || ctxMoveUp) {
                const int count = contextMenuList_->count();
                int row = contextMenuList_->currentRow();
                row = row < 0 ? (ctxMoveDown ? 0 : count - 1)
                              : qBound(0, row + (ctxMoveDown ? 1 : -1), count - 1);
                contextMenuList_->setCurrentRow(row);
                return true;
            }
            return QWidget::eventFilter(watched, event);
        }

        // Query or results list
        if (watched == query_ || watched == results_) {
            if (watched == query_ && shiftOnly
                && keyEvent->key() == Qt::Key_Backspace) {
                query_->clear();
                return true;
            }
            if (shiftEnter && resultsVisible_
                && (currentResult() || (showingEngines_ && results_->currentItem()))) {
                if (contextMenuVisible_) {
                    hideContextMenu();
                } else {
                    showContextMenu();
                }
                return true;
            }

            const bool shiftNavigation = modifiers.testFlag(Qt::ShiftModifier)
                && !(modifiers & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
            const bool arrowNavigation = !(modifiers & (Qt::ShiftModifier
                                                         | Qt::ControlModifier
                                                         | Qt::AltModifier
                                                         | Qt::MetaModifier));
            const bool moveDown = (shiftNavigation && keyEvent->key() == Qt::Key_N)
                                  || (arrowNavigation && keyEvent->key() == Qt::Key_Down);
            const bool moveUp = (shiftNavigation && keyEvent->key() == Qt::Key_P)
                                || (arrowNavigation && keyEvent->key() == Qt::Key_Up);
            if (moveDown || moveUp) {
                moveSelection(moveDown ? 1 : -1);
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void LocalSearchWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const ThemeTokens& tokens = ThemeManager::tokens();
    const int sp = scaled(kShadowPad);
    const int sy = scaled(kShadowOffY);
    const qreal radius = scaled(kCornerRadius);
    const QRectF surface(sp + 0.5, sp + 0.5,
                         width() - sp * 2 - 1.0,
                         height() - sp * 2 - sy - 1.0);

    // 多层柔和阴影：拉开与下方窗口的层级，避免纯白卡片融进桌面
    const bool dark = tokens.theme == AppTheme::Dark;
    struct Layer { qreal spread; qreal yOff; int alpha; };
    const Layer layers[] = {
        { qreal(scaled(5)), qreal(scaled(1)), dark ? 28 : 12 },
        { qreal(scaled(2)), qreal(scaled(1)), dark ? 22 : 14 },
        { qreal(scaled(1)), qreal(0), dark ? 16 : 18 },
    };
    painter.setPen(Qt::NoPen);
    for (const auto& layer : layers) {
        // 底部扩散减半，避免下边阴影明显重于上边/侧边
        QRectF shadowRect = surface.adjusted(-layer.spread, -layer.spread * 0.5,
                                             layer.spread, layer.spread * 0.45);
        shadowRect.translate(0, layer.yOff);
        QColor shadow = tokens.shadow;
        shadow.setAlpha(layer.alpha);
        painter.setBrush(shadow);
        painter.drawRoundedRect(shadowRect,
                                radius + layer.spread * 0.35,
                                radius + layer.spread * 0.35);
    }

    painter.setPen(QPen(tokens.borderStrong, 1));
    painter.setBrush(tokens.surfaceRaised);
    painter.drawRoundedRect(surface, radius, radius);
}

void LocalSearchWidget::startSearch()
{
    pendingQuery_ = query_->text().trimmed();
    if (pendingQuery_.isEmpty()) {
        showingEngines_ = false;
        currentResults_.clear();
        results_->clear();
        resultCount_->clear();
        if (contextMenuVisible_) hideContextMenu();
        setResultsVisible(false);
        return;
    }
    if (isWebSearchMode(pendingQuery_)) {
        if (contextMenuVisible_) hideContextMenu();
        showSearchEngines(pendingQuery_);
        return;
    }
    showingEngines_ = false;
    if (contextMenuVisible_) hideContextMenu();
    if (searchWatcher_->isRunning()) {
        rerunPending_ = true;
        return;
    }
    const QString text = pendingQuery_;
    auto future = QtConcurrent::run([this, text]() -> QVector<SearchResult> {
        SearchQuery query;
        query.text = text;
        query.limit = 60;
        query.pinyinEnabled = pinyinEnabled_;
        return aggregator_->search(query);
    });
    searchWatcher_->setFuture(future);
    connect(searchWatcher_, &QFutureWatcher<QVector<SearchResult>>::finished,
            this, [this, text]() {
        const QVector<SearchResult> results = searchWatcher_->result();
        if (query_->text().trimmed() == text) showResults(results);
        if (rerunPending_ || pendingQuery_ != text) {
            rerunPending_ = false;
            startSearch();
        }
    }, Qt::SingleShotConnection);
}

void LocalSearchWidget::showResults(const QVector<SearchResult>& results)
{
    if (contextMenuVisible_) hideContextMenu();

    const int previousRow = results_->currentRow();
    const SearchResult* previousResult = currentResult();
    const qint64 previousId = previousResult ? previousResult->id : 0;
    const SearchItemType previousType = previousResult
        ? previousResult->type : SearchItemType::File;
    const QString previousPath = previousResult ? previousResult->path : QString();

    if (haveSameDisplayedResults(currentResults_, results)) {
        currentResults_ = results;
        return;
    }

    currentResults_ = results;
    results_->setUpdatesEnabled(false);
    {
        const QSignalBlocker blocker(results_);
        results_->clear();
    }
    for (const SearchResult& result : currentResults_) {
        QString type;
        if (result.type == SearchItemType::Application) type = QStringLiteral("应用");
        else if (result.type == SearchItemType::Bookmark) type = QStringLiteral("书签");
        else if (result.type == SearchItemType::Directory) type = QStringLiteral("目录");
        else type = QStringLiteral("文件");
        const QString bookmarkGroup = result.type == SearchItemType::Bookmark
            ? bookmarkGroupLabel(result.parentPath) : QString();
        const QString metadata = bookmarkGroup.isEmpty()
            ? type : QStringLiteral("%1  ·  %2").arg(type, bookmarkGroup);
        auto* item = new QListWidgetItem(iconForResult(result),
                                         QStringLiteral("%1\n%2")
                                             .arg(result.name, result.path), results_);
        item->setData(NameRole, result.name);
        item->setData(PathRole, result.path);
        item->setData(MetadataRole, metadata);
        item->setToolTip(bookmarkGroup.isEmpty()
                             ? result.path
                             : QStringLiteral("%1\n%2").arg(result.path, bookmarkGroup));
        item->setSizeHint(QSize(0, scaled(46)));
    }

    int selectedRow = -1;
    for (qsizetype row = 0; row < currentResults_.size(); ++row) {
        const SearchResult& result = currentResults_[row];
        if ((previousId != 0 && result.id == previousId)
            || (!previousPath.isEmpty() && result.type == previousType
                && result.path.compare(previousPath, Qt::CaseInsensitive) == 0)) {
            selectedRow = static_cast<int>(row);
            break;
        }
    }
    if (selectedRow < 0 && !currentResults_.isEmpty()) {
        selectedRow = qBound(0, previousRow, currentResults_.size() - 1);
    }
    if (selectedRow >= 0) {
        results_->setCurrentRow(selectedRow);
        results_->scrollToItem(results_->item(selectedRow), QAbstractItemView::EnsureVisible);
    }
    setResultsVisible(!currentResults_.isEmpty());
    resultCount_->setText(currentResults_.isEmpty()
                              ? QString()
                              : QStringLiteral("找到 %1 项").arg(currentResults_.size()));
    results_->setUpdatesEnabled(true);
    results_->viewport()->update();
}

QIcon LocalSearchWidget::iconForResult(const SearchResult& result)
{
    if (result.type == SearchItemType::Bookmark) return bookmarkIcon(result);
    const QString target = result.launchTarget.isEmpty() ? result.path : result.launchTarget;
    const QString cacheKey = QStringLiteral("%1|%2")
                                 .arg(static_cast<int>(result.type))
                                 .arg(target);
    const auto cached = iconCache_.constFind(cacheKey);
    if (cached != iconCache_.cend()) return cached.value();

    QIcon icon;
#ifdef Q_OS_WIN
    if (result.type == SearchItemType::Application) {
        const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        icon = windowsShellIcon(target);
        if (icon.isNull() && target != result.path) icon = windowsShellIcon(result.path);
        if (SUCCEEDED(comResult)) CoUninitialize();
    }
#endif

    QFileIconProvider fileIcons;
    if (icon.isNull() && QFileInfo::exists(result.path)) {
        icon = fileIcons.icon(QFileInfo(result.path));
    }
    if (icon.isNull() && result.type == SearchItemType::Directory) {
        icon = fileIcons.icon(QFileIconProvider::Folder);
    }
    if (icon.isNull() && result.type == SearchItemType::Application) {
        icon = QApplication::style()->standardIcon(QStyle::SP_DesktopIcon);
    }
    if (icon.isNull()) icon = fileIcons.icon(QFileIconProvider::File);

    if (iconCache_.size() >= 512) iconCache_.clear();
    iconCache_.insert(cacheKey, icon);
    return icon;
}

QIcon LocalSearchWidget::bookmarkIcon(const SearchResult& result)
{
    const QUrl pageUrl(result.launchTarget.isEmpty() ? result.path : result.launchTarget);
    const QString domain = pageUrl.host().toLower();
    if (domain.isEmpty()) {
        return browserIconForBookmark(result);
    }
    const auto cached = faviconCache_.constFind(domain);
    if (cached != faviconCache_.cend()) return cached.value();

    const QString fileName = QString::fromLatin1(QUrl::toPercentEncoding(domain))
                             + QStringLiteral(".v2.png");
    const QString cachePath = QDir(faviconCacheDirectory_).filePath(fileName);
    const QFileInfo cacheInfo(cachePath);
    if (cacheInfo.exists()
        && cacheInfo.lastModified().secsTo(QDateTime::currentDateTime())
               < kFaviconCacheMaxAgeSeconds) {
        const QIcon diskIcon(cachePath);
        if (!diskIcon.isNull()) {
            faviconCache_.insert(domain, diskIcon);
            return diskIcon;
        }
    }

    requestFavicon(pageUrl, domain);
    return browserIconForBookmark(result);
}

QIcon LocalSearchWidget::browserIconForBookmark(const SearchResult& result)
{
    const bool fromEdge = result.parentPath.startsWith(
        QStringLiteral("Edge"), Qt::CaseInsensitive);
    const QString cacheKey = fromEdge ? QStringLiteral("browser|edge")
                                      : QStringLiteral("browser|chrome");
    const auto cached = iconCache_.constFind(cacheKey);
    if (cached != iconCache_.cend()) return cached.value();

    QIcon icon;
#ifdef Q_OS_WIN
    QStringList candidates;
    const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    const QString programFiles = qEnvironmentVariable("ProgramFiles");
    const QString programFilesX86 = qEnvironmentVariable("ProgramFiles(x86)");
    if (fromEdge) {
        candidates << programFiles + QStringLiteral("/Microsoft/Edge/Application/msedge.exe")
                   << programFilesX86 + QStringLiteral("/Microsoft/Edge/Application/msedge.exe")
                   << localAppData + QStringLiteral("/Microsoft/Edge/Application/msedge.exe");
    } else {
        candidates << programFiles + QStringLiteral("/Google/Chrome/Application/chrome.exe")
                   << programFilesX86 + QStringLiteral("/Google/Chrome/Application/chrome.exe")
                   << localAppData + QStringLiteral("/Google/Chrome/Application/chrome.exe");
    }
    for (const QString& candidate : candidates) {
        if (!QFileInfo::exists(candidate)) continue;
        icon = windowsShellIcon(candidate);
        if (icon.isNull()) {
            QFileIconProvider fileIcons;
            icon = fileIcons.icon(QFileInfo(candidate));
        }
        if (!icon.isNull()) break;
    }
#endif
    if (icon.isNull()) {
        icon = webPageFallbackIcon();
    }
    iconCache_.insert(cacheKey, icon);
    return icon;
}

void LocalSearchWidget::requestFavicon(const QUrl& pageUrl, const QString& domain)
{
    if (pendingFavicons_.contains(domain)) return;
    pendingFavicons_.insert(domain);

    QUrl fallbackUrl;
    fallbackUrl.setScheme(pageUrl.scheme());
    fallbackUrl.setHost(pageUrl.host());
    fallbackUrl.setPort(pageUrl.port());
    fallbackUrl.setPath(QStringLiteral("/favicon.ico"));

    QNetworkRequest request(pageUrl);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("ntscreenshot/1.0"));
    QNetworkReply* reply = network_->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, domain, fallbackUrl]() {
        QByteArray pageData = reply->error() == QNetworkReply::NoError
            ? reply->readAll() : QByteArray();
        if (pageData.size() > 2 * 1024 * 1024) pageData.clear();
        const QUrl finalPageUrl = reply->url();
        reply->deleteLater();
        const QUrl declaredUrl = declaredFaviconUrl(pageData, finalPageUrl);
        requestFaviconImage(declaredUrl.isValid() ? declaredUrl : fallbackUrl,
                            fallbackUrl, domain, declaredUrl.isValid());
    });
}

void LocalSearchWidget::requestFaviconImage(const QUrl& iconUrl, const QUrl& fallbackUrl,
                                           const QString& domain, bool allowFallback)
{
    QNetworkRequest request(iconUrl);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("ntscreenshot/1.0"));
    QNetworkReply* reply = network_->get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, iconUrl, fallbackUrl, domain, allowFallback]() {
        const QByteArray data = reply->error() == QNetworkReply::NoError
            ? reply->readAll() : QByteArray();
        reply->deleteLater();

        QPixmap pixmap;
        if (!data.isEmpty() && data.size() <= 1024 * 1024 && pixmap.loadFromData(data)) {
            pendingFavicons_.remove(domain);
            applyFavicon(domain, pixmap);
            return;
        }
        if (allowFallback && iconUrl != fallbackUrl) {
            requestFaviconImage(fallbackUrl, fallbackUrl, domain, false);
            return;
        }
        pendingFavicons_.remove(domain);
    });
}

void LocalSearchWidget::applyFavicon(const QString& domain, const QPixmap& pixmap)
{
    const QIcon icon(pixmap);
    faviconCache_.insert(domain, icon);

    const QString fileName = QString::fromLatin1(QUrl::toPercentEncoding(domain))
                             + QStringLiteral(".v2.png");
    QSaveFile file(QDir(faviconCacheDirectory_).filePath(fileName));
    if (file.open(QIODevice::WriteOnly)) {
        pixmap.save(&file, "PNG");
        file.commit();
    }

    for (qsizetype row = 0; row < currentResults_.size(); ++row) {
        const SearchResult& result = currentResults_.at(row);
        const QUrl resultUrl(result.launchTarget.isEmpty() ? result.path : result.launchTarget);
        if (result.type == SearchItemType::Bookmark
            && resultUrl.host().compare(domain, Qt::CaseInsensitive) == 0
            && results_->item(static_cast<int>(row))) {
            results_->item(static_cast<int>(row))->setIcon(icon);
        }
    }
}

void LocalSearchWidget::setResultsVisible(bool visible)
{
    resultsVisible_ = visible;
    resultsPanel_->setVisible(visible);
    rootLayout_->invalidate();
    rootLayout_->activate();
    emit preferredHeightChanged(visible ? expandedHeight() : compactHeight());
}

void LocalSearchWidget::showSearchEngines(const QString& query)
{
    const QString terms = query.mid(1).trimmed();
    const auto engines = SettingModel::localSearchWebEngines();
    const int iconSize = results_->iconSize().width();

    const int previousRow = results_->currentRow();
    int previousEngineIndex = -1;
    if (showingEngines_ && previousRow >= 0 && previousRow < results_->count()) {
        QListWidgetItem* prevItem = results_->item(previousRow);
        if (prevItem) previousEngineIndex = prevItem->data(Qt::UserRole).toInt();
    }

    // List content is fixed; skip rebuild while typing the search query.
    if (!showingEngines_ || results_->count() != engines.size()) {
        int preferredEngineIndex = -1;
        for (int i = 0; i < engines.size(); ++i) {
            if (engines[i].baseUrl == webSearchUrl_) {
                preferredEngineIndex = i;
                break;
            }
        }

        currentResults_.clear();
        results_->clear();

        for (int idx = 0; idx < engines.size(); ++idx) {
            const auto& engine = engines[idx];
            auto* item = new QListWidgetItem(
                searchEngineIcon(engine, iconSize),
                QStringLiteral("%1\n%2").arg(engine.name, engine.slogan),
                results_);
            item->setToolTip(QStringLiteral("%1\n%2").arg(engine.name, engine.slogan));
            item->setData(Qt::UserRole, idx);
            item->setData(NameRole, engine.name);
            item->setData(PathRole, engine.slogan);
            item->setData(MetadataRole, QStringLiteral("搜索引擎"));
            item->setSizeHint(QSize(0, scaled(46)));
        }

        int selectedRow = 0;
        const int targetEngineIndex = previousEngineIndex >= 0
            ? previousEngineIndex
            : preferredEngineIndex;
        if (targetEngineIndex >= 0 && targetEngineIndex < results_->count()) {
            selectedRow = targetEngineIndex;
        }
        if (results_->count() > 0) {
            results_->setCurrentRow(selectedRow);
        }
    }

    showingEngines_ = true;
    setResultsVisible(results_->count() > 0);
    resultCount_->setText(terms.isEmpty()
        ? QStringLiteral("选择引擎并输入关键词")
        : QStringLiteral("回车搜索"));
}

int LocalSearchWidget::compactHeight() const
{
    return scaled(kShadowPad + kInnerPad + kQueryHeight + kInnerPad + kShadowPad + kShadowOffY);
}

int LocalSearchWidget::expandedHeight() const
{
    constexpr int kMaxVisibleItems = 7;
    const int itemHeight = scaled(46);
    const int resultsTopMargin = scaled(6);
    const int footerHeight = scaled(29);
    const int count = showingEngines_ ? results_->count()
                                      : static_cast<int>(currentResults_.size());
    const int visibleItems = qMin(count, kMaxVisibleItems);
    const int contentHeight = compactHeight() + resultsTopMargin
                              + visibleItems * itemHeight + footerHeight;
    return contentHeight;
}

int LocalSearchWidget::contextMenuHeight() const
{
    const int count = contextMenuList_->count();
    if (count <= 0) return compactHeight();
    const int resultsTopMargin = scaled(6);
    // 右键菜单显示时计数标签已隐藏，不再占用高度
    return compactHeight() + resultsTopMargin + count * scaled(42) + scaled(29);
}

int LocalSearchWidget::scaled(int value) const
{
    return qRound(value * scaleFactor_);
}

const SearchResult* LocalSearchWidget::currentResult() const
{
    const int row = results_->currentRow();
    return row >= 0 && row < currentResults_.size() ? &currentResults_[row] : nullptr;
}

void LocalSearchWidget::openCurrent()
{
    if (isWebSearchMode(query_->text())) {
        const QString terms = webSearchTerms(query_->text());
        if (terms.isEmpty()) return;
        QString engineUrl = webSearchUrl_;
        QString engineParam = webSearchQueryParam_;
        if (showingEngines_) {
            QListWidgetItem* currentItem = results_->currentItem();
            if (currentItem) {
                const int engineIdx = currentItem->data(Qt::UserRole).toInt();
                const auto engines = SettingModel::localSearchWebEngines();
                if (engineIdx >= 0 && engineIdx < engines.size()) {
                    engineUrl = engines[engineIdx].baseUrl;
                    engineParam = engines[engineIdx].queryParam;
                }
            }
        }
        QUrl url(engineUrl);
        QUrlQuery urlQuery;
        urlQuery.addQueryItem(engineParam, terms);
        url.setQuery(urlQuery);
        if (QDesktopServices::openUrl(url)) emit requestHide();
        return;
    }

    const SearchResult* result = currentResult();
    if (!result) return;
    const QString target = result->launchTarget.isEmpty() ? result->path : result->launchTarget;
    if (result->type == SearchItemType::Bookmark) {
        if (!QDesktopServices::openUrl(QUrl(target))) {
            resultCount_->setText(QStringLiteral("无法使用默认浏览器打开书签"));
            return;
        }
        store_->recordLaunch(result->id);
        emit requestHide();
        return;
    }
#ifdef Q_OS_WIN
    if (result->type == SearchItemType::Application &&
        (target.startsWith(QStringLiteral("shell:"), Qt::CaseInsensitive) ||
         !QFileInfo::exists(target))) {
        if (!QProcess::startDetached(QStringLiteral("explorer.exe"), {target})) {
            resultCount_->setText(QStringLiteral("无法启动应用"));
            return;
        }
        store_->recordLaunch(result->id);
        emit requestHide();
        return;
    }
#endif
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(target))) {
        resultCount_->setText(QStringLiteral("无法打开，索引已标记为需要刷新"));
        return;
    }
    store_->recordLaunch(result->id);
    emit requestHide();
}

void LocalSearchWidget::locateCurrent()
{
    const SearchResult* result = currentResult();
    if (!result) return;
#ifdef Q_OS_WIN
    if (result->type == SearchItemType::Directory) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(result->path));
    } else {
        QProcess::startDetached(QStringLiteral("explorer.exe"),
                                {QStringLiteral("/select,"), QDir::toNativeSeparators(result->path)});
    }
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(result->parentPath));
#endif
}

void LocalSearchWidget::copyCurrentPath()
{
    const SearchResult* result = currentResult();
    if (result) QApplication::clipboard()->setText(result->path);
}

bool LocalSearchWidget::runCurrentAsAdmin()
{
    const SearchResult* result = currentResult();
    if (!result || !canRunAsAdmin(*result)) return false;
    const QString target = result->launchTarget.isEmpty() ? result->path : result->launchTarget;
    if (!Util::shellExecute(target, QStringLiteral("runas"))) {
        resultCount_->setText(QStringLiteral("无法以管理员身份运行"));
        return false;
    }
    store_->recordLaunch(result->id);
    return true;
}

bool LocalSearchWidget::openCurrentInTerminal()
{
    const SearchResult* result = currentResult();
    if (!result || result->type == SearchItemType::Bookmark) return false;
    const QString directory = terminalDirectoryFor(*result);
    if (directory.isEmpty() || !QDir(directory).exists()) {
        resultCount_->setText(QStringLiteral("无法打开终端：目录不存在"));
        return false;
    }
#ifdef Q_OS_WIN
    if (QProcess::startDetached(QStringLiteral("wt.exe"),
                                {QStringLiteral("-d"), directory})) {
        return true;
    }
    const QString command = QStringLiteral("cd /d \"%1\"")
                                .arg(QDir::toNativeSeparators(directory));
    if (QProcess::startDetached(QStringLiteral("cmd.exe"),
                                {QStringLiteral("/k"), command})) {
        return true;
    }
    resultCount_->setText(QStringLiteral("无法打开终端"));
    return false;
#else
    if (QProcess::startDetached(QStringLiteral("x-terminal-emulator"),
                                {QStringLiteral("--working-directory"), directory})
        || QProcess::startDetached(QStringLiteral("gnome-terminal"),
                                   {QStringLiteral("--working-directory"), directory})) {
        return true;
    }
    resultCount_->setText(QStringLiteral("无法打开终端"));
    return false;
#endif
}

void LocalSearchWidget::openCurrentEngineHome()
{
    if (!showingEngines_) return;
    QListWidgetItem* item = results_->currentItem();
    if (!item) return;
    const int engineIdx = item->data(Qt::UserRole).toInt();
    const auto engines = SettingModel::localSearchWebEngines();
    if (engineIdx < 0 || engineIdx >= engines.size()) return;
    if (QDesktopServices::openUrl(QUrl(engines[engineIdx].baseUrl))) emit requestHide();
}

void LocalSearchWidget::copyCurrentEngineUrl()
{
    if (!showingEngines_) return;
    QListWidgetItem* item = results_->currentItem();
    if (!item) return;
    const int engineIdx = item->data(Qt::UserRole).toInt();
    const auto engines = SettingModel::localSearchWebEngines();
    if (engineIdx < 0 || engineIdx >= engines.size()) return;

    const auto& engine = engines[engineIdx];
    const QString terms = webSearchTerms(query_->text());
    if (terms.isEmpty()) {
        QApplication::clipboard()->setText(engine.baseUrl);
        return;
    }
    QUrl url(engine.baseUrl);
    QUrlQuery urlQuery;
    urlQuery.addQueryItem(engine.queryParam, terms);
    url.setQuery(urlQuery);
    QApplication::clipboard()->setText(url.toString());
}

void LocalSearchWidget::setCurrentEngineDefault()
{
    if (!showingEngines_) return;
    QListWidgetItem* item = results_->currentItem();
    if (!item) return;
    const int engineIdx = item->data(Qt::UserRole).toInt();
    const auto engines = SettingModel::localSearchWebEngines();
    if (engineIdx < 0 || engineIdx >= engines.size()) return;
    const auto& engine = engines[engineIdx];
    setWebSearchEngine(engine.baseUrl, engine.queryParam);
    emit requestSetWebSearchEngine(engine.id);
    resultCount_->setText(QStringLiteral("已设为默认：%1").arg(engine.name));
}

void LocalSearchWidget::addContextAction(const QString& text, const QIcon& icon, int actionId)
{
    auto* item = new QListWidgetItem(contextMenuList_);
    item->setText(text);
    item->setIcon(icon);
    item->setData(Qt::UserRole, actionId);
    item->setSizeHint(QSize(0, scaled(40)));
}

void LocalSearchWidget::showContextMenu()
{
    contextMenuList_->clear();

    if (showingEngines_) {
        QListWidgetItem* currentItem = results_->currentItem();
        if (!currentItem) return;
        const int engineIdx = currentItem->data(Qt::UserRole).toInt();
        const auto engines = SettingModel::localSearchWebEngines();
        if (engineIdx < 0 || engineIdx >= engines.size()) return;

        const auto& engine = engines[engineIdx];
        const QString terms = webSearchTerms(query_->text());
        addContextAction(QStringLiteral("打开主页"),
                         contextActionIcon(QStringLiteral("ctx_open.png")),
                         ActionOpenEngineHome);
        addContextAction(terms.isEmpty()
                             ? QStringLiteral("复制主页链接")
                             : QStringLiteral("复制搜索链接"),
                         contextActionIcon(QStringLiteral("ctx_copy.png")),
                         ActionCopyEngineUrl);
        if (engine.baseUrl != webSearchUrl_) {
            addContextAction(QStringLiteral("设为默认搜索引擎"),
                             contextActionIcon(QStringLiteral("ok.png")),
                             ActionSetDefaultEngine);
        }
    } else {
        const SearchResult* result = currentResult();
        if (!result) return;

        addContextAction(QStringLiteral("打开"),
                         contextActionIcon(QStringLiteral("ctx_open.png")),
                         ActionOpen);
        addContextAction(result->type == SearchItemType::Bookmark
                             ? QStringLiteral("复制链接")
                             : QStringLiteral("复制完整路径"),
                         contextActionIcon(QStringLiteral("ctx_copy.png")),
                         ActionCopyPath);

        if (result->type != SearchItemType::Bookmark) {
            addContextAction(QStringLiteral("在资源管理器中定位"),
                             contextActionIcon(QStringLiteral("ctx_folder.png")),
                             ActionLocate);
            addContextAction(result->type == SearchItemType::Directory
                                 ? QStringLiteral("在终端中打开")
                                 : QStringLiteral("在终端中打开所在目录"),
                             contextActionIcon(QStringLiteral("ctx_terminal.png")),
                             ActionOpenTerminal);
            if (canRunAsAdmin(*result)) {
                addContextAction(QStringLiteral("以管理员身份运行"),
                                 contextActionIcon(QStringLiteral("shield.png")),
                                 ActionRunAsAdmin);
            }
        }
    }

    if (contextMenuList_->count() <= 0) return;

    contextMenuList_->setCurrentRow(0);
    stackedWidget_->setCurrentIndex(1);
    contextMenuVisible_ = true;
    contextMenuList_->setFocus(Qt::ShortcutFocusReason);
    emit preferredHeightChanged(contextMenuHeight());
}

void LocalSearchWidget::hideContextMenu()
{
    if (!contextMenuVisible_) return;
    contextMenuVisible_ = false;
    contextMenuList_->clear();
    stackedWidget_->setCurrentIndex(0);
    query_->setFocus(Qt::ShortcutFocusReason);
    emit preferredHeightChanged(expandedHeight());
}

void LocalSearchWidget::executeContextAction(int actionId)
{
    if (!contextMenuVisible_) return;
    // Switch back before executing so currentResult() resolves correctly.
    contextMenuVisible_ = false;
    contextMenuList_->clear();
    stackedWidget_->setCurrentIndex(0);

    bool shouldHide = true;
    switch (actionId) {
    case ActionOpen:
        openCurrent();
        shouldHide = false; // openCurrent hides on success
        break;
    case ActionLocate:
        locateCurrent();
        break;
    case ActionCopyPath:
        copyCurrentPath();
        break;
    case ActionRunAsAdmin:
        shouldHide = runCurrentAsAdmin();
        break;
    case ActionOpenTerminal:
        shouldHide = openCurrentInTerminal();
        break;
    case ActionOpenEngineHome:
        openCurrentEngineHome();
        shouldHide = false;
        break;
    case ActionCopyEngineUrl:
        copyCurrentEngineUrl();
        break;
    case ActionSetDefaultEngine:
        setCurrentEngineDefault();
        shouldHide = false;
        break;
    }
    if (shouldHide) {
        emit requestHide();
        return;
    }
    if (actionId == ActionSetDefaultEngine
        || actionId == ActionRunAsAdmin
        || actionId == ActionOpenTerminal) {
        query_->setFocus(Qt::ShortcutFocusReason);
        emit preferredHeightChanged(expandedHeight());
    }
}

void LocalSearchWidget::moveSelection(int offset)
{
    const int count = results_->count();
    if (!resultsVisible_ || count <= 0 || offset == 0) return;
    int row = results_->currentRow();
    row = row < 0 ? (offset > 0 ? 0 : count - 1)
                  : qBound(0, row + offset, count - 1);
    results_->setCurrentRow(row);
    results_->scrollToItem(results_->item(row), QAbstractItemView::EnsureVisible);
    query_->setFocus(Qt::ShortcutFocusReason);
}
