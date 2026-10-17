#include "HistoryWindow.h"

#include "core/foundation/TimeUtil.h"
#include "core/platform/Util.h"
#include "core/theme/ThemeIcon.h"
#include "core/theme/ThemeManager.h"

#include <QApplication>
#include <QClipboard>
#include <QCache>
#include <QCryptographicHash>
#include <QSignalBlocker>
#include <QCloseEvent>
#include <QCursor>
#include <QGuiApplication>
#include <QHash>
#include <QHideEvent>
#include <QImage>
#include <QKeyEvent>
#include <QMenu>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScreen>
#include <QShowEvent>
#include <QStyledItemDelegate>
#include <QTextLayout>
#include <QTextOption>
#include <QVBoxLayout>

namespace {

constexpr int kHeaderHeight = 34;
constexpr int kSearchHeight = 24;
constexpr int kMaxPreviewLines = 3;
constexpr int kNumberWidth = 28;
constexpr int kTimeWidth = 40;
constexpr int kFavoriteRole = Qt::UserRole + 1;

void DrawStar(QPainter& painter, const QRectF& rect, const QColor& color, bool filled)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(rect.topLeft());
    painter.scale(rect.width() / 16.0, rect.height() / 16.0);
    QPainterPath star;
    star.moveTo(8, 1);
    star.lineTo(10, 5.8);
    star.lineTo(15, 6.2);
    star.lineTo(11.2, 9.5);
    star.lineTo(12.4, 14.7);
    star.lineTo(8, 12);
    star.lineTo(3.6, 14.7);
    star.lineTo(4.8, 9.5);
    star.lineTo(1, 6.2);
    star.lineTo(6, 5.8);
    star.closeSubpath();
    painter.setPen(QPen(color, 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(filled ? QBrush(color) : Qt::NoBrush);
    painter.drawPath(star);
    painter.restore();
}

QIcon ViewIcon(bool favorites, int size)
{
    QIcon icon;
    const auto& tokens = ThemeManager::tokens();
    for (bool checked : {false, true}) {
        const qreal dpr = qApp->devicePixelRatio();
        QPixmap pixmap(qRound(size * dpr), qRound(size * dpr));
        pixmap.setDevicePixelRatio(dpr);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        const QColor color = checked ? tokens.accent : tokens.textSecondary;
        if (favorites) {
            DrawStar(painter, QRectF(0, 0, size, size), color, checked);
        } else {
            painter.setRenderHint(QPainter::Antialiasing);
            painter.scale(size / 16.0, size / 16.0);
            painter.setPen(QPen(color, 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawRoundedRect(QRectF(3, 3.5, 10, 11.5), 1.5, 1.5);
            painter.setBrush(checked ? tokens.accentSubtle : tokens.canvas);
            painter.drawRoundedRect(QRectF(5.5, 1, 5, 4), 1, 1);
            painter.drawLine(QPointF(5.5, 8), QPointF(10.5, 8));
            painter.drawLine(QPointF(5.5, 11), QPointF(9, 11));
        }
        painter.end();
        icon.addPixmap(pixmap, QIcon::Normal, checked ? QIcon::On : QIcon::Off);
    }
    return icon;
}

QIcon CloudIcon(int size) {
    QIcon icon;
    const auto& tokens = ThemeManager::tokens();
    for (bool checked : {false, true}) {
        const qreal dpr = qApp->devicePixelRatio();
        QPixmap pixmap(qRound(size * dpr), qRound(size * dpr));
        pixmap.setDevicePixelRatio(dpr);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.scale(size / 16.0, size / 16.0);
        painter.setPen(QPen(checked ? tokens.accent : tokens.textSecondary, 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPainterPath cloud;
        cloud.moveTo(4, 12);
        cloud.cubicTo(0, 12, 0, 6, 4, 6);
        cloud.cubicTo(4, 0, 12, 0, 12, 6);
        cloud.cubicTo(16, 6, 16, 12, 12, 12);
        painter.drawPath(cloud);
        painter.drawLine(QPointF(8, 14), QPointF(8, 7));
        painter.drawLine(QPointF(5.5, 9.5), QPointF(8, 7));
        painter.drawLine(QPointF(10.5, 9.5), QPointF(8, 7));
        painter.end();
        icon.addPixmap(pixmap, QIcon::Normal, checked ? QIcon::On : QIcon::Off);
    }
    return icon;
}

int Scaled(int value, qreal scaleFactor) {
    return qRound(value * scaleFactor);
}

QColor WithAlpha(QColor color, int alpha) {
    color.setAlpha(alpha);
    return color;
}

QStringList QueryTerms(const QString& query) {
    return query.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
}

bool MatchesAllTerms(const QString& text, const QStringList& terms) {
    for (const QString& term : terms) {
        if (!text.contains(term, Qt::CaseInsensitive)) {
            return false;
        }
    }
    return true;
}

QString CssColor(const QColor& color) {
    return color.name(QColor::HexArgb);
}

class HistoryDelegate final : public QStyledItemDelegate {
public:
    HistoryDelegate(const ClipboardHistory* history, qreal scaleFactor, QObject* parent = nullptr)
        : QStyledItemDelegate(parent), history_(history), scaleFactor_(scaleFactor) {}

    void setHistory(const ClipboardHistory* history) {
        if (history_ != history) {
            markerItems_.clear(); favoriteMarkers_.clear(); sizeCache_.clear();
        }
        history_ = history;
    }

    void UpdateFavoriteMarkers() {
        const auto& items = history_->Items();
        const auto same = [](const ClipItem& a, const ClipItem& b) {
            return a.kind == b.kind && a.favorite == b.favorite && a.text == b.text
                && a.data == b.data && a.width == b.width && a.height == b.height && a.pixelHash == b.pixelHash;
        };
        if (items.size() == markerItems_.size() && std::equal(items.begin(), items.end(), markerItems_.begin(), same)) return;
        markerItems_ = items; // QString/QByteArray share storage; no image/text payload copies.
        sizeCache_.clear();
        favoriteMarkers_.assign(items.size(), false);
        for (size_t i = 0; i < items.size(); ++i) {
            if (items[i].favorite) continue;
            for (const auto& saved : items) {
                if (!saved.favorite || saved.kind != items[i].kind) continue;
                if (SameForMarker(saved, items[i])) { favoriteMarkers_[i] = true; break; }
            }
        }
    }

    bool FavoriteMarked(size_t index) const {
        return index < favoriteMarkers_.size() && favoriteMarkers_[index];
    }

    void setBackgroundAlpha(int alpha) { backgroundAlpha_ = alpha; }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        const auto* view = qobject_cast<const QAbstractItemView*>(parent());
        const int width = view ? view->viewport()->width() : s(HistoryWindow::kPopupWidth);
        const int historyIndex = index.data(Qt::UserRole).toInt();
        const bool marked = index.data(kFavoriteRole).toBool();
        const auto cached = sizeCache_.constFind(historyIndex);
        if (cached != sizeCache_.constEnd() && cached->width == width && cached->row == index.row()
            && cached->font == option.font && cached->marked == marked) return cached->size;
        int lineCount = 1;
        if (history_ && historyIndex >= 0 && historyIndex < static_cast<int>(history_->Items().size())) {
            const ClipItem& item = history_->Items()[static_cast<size_t>(historyIndex)];
            if (item.kind == ClipKind::Image) {
                lineCount = kMaxPreviewLines;
            } else {
                QTextLayout layout(PreviewText(item.text), option.font);
                lineCount = LayoutPreview(layout, ContentWidth(width, option.font, index.row(), index.data(kFavoriteRole).toBool())).lines;
            }
        }
        const QSize result(width, lineCount * QFontMetrics(option.font).height() + s(10));
        sizeCache_.insert(historyIndex, {width, index.row(), option.font, marked, result});
        return result;
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::TextAntialiasing, true);

        const QPalette& palette = option.palette;
        const bool selected = option.state.testFlag(QStyle::State_Selected);
        const QRect rowRect = option.rect;

        const ThemeTokens& tokens = ThemeManager::tokens();
        if (selected) {
            painter->fillRect(rowRect.adjusted(s(1), 0, -s(1), 0),
                              WithAlpha(tokens.accentSubtle, backgroundAlpha_));
        }

        const int historyIndex = index.data(Qt::UserRole).toInt();
        if (!history_ || historyIndex < 0 ||
            historyIndex >= static_cast<int>(history_->Items().size())) {
            painter->restore();
            return;
        }

        const ClipItem& item = history_->Items()[static_cast<size_t>(historyIndex)];
        const QColor primary = selected ? tokens.textPrimary
                                        : palette.color(QPalette::Text);
        const QColor secondary = selected ? tokens.textSecondary
                                          : palette.color(QPalette::PlaceholderText);

        const QFont numberFont = NumberFont(option.font);
        painter->setFont(numberFont);
        painter->setPen(secondary);
        const QString numberText = QString::number(index.row() + 1);
        const int numberWidth = QFontMetrics(numberFont).horizontalAdvance(numberText);
        const QRect numberRect(rowRect.left() + s(2), rowRect.top() + s(6),
                               numberWidth, rowRect.height() - s(12));
        painter->drawText(numberRect, Qt::AlignTop | Qt::AlignLeft,
                          numberText);

        const QRect timeRect(rowRect.right() - s(kTimeWidth), rowRect.top() + s(5),
                             s(kTimeWidth - 6), rowRect.height() - s(10));
        painter->setPen(secondary);
        painter->drawText(timeRect, Qt::AlignBottom | Qt::AlignRight,
                          TimeUtil::FormatRelativeTime(item.capturedAt));

        const bool favoriteMarker = index.data(kFavoriteRole).toBool();
        if (favoriteMarker) {
            DrawStar(*painter, QRectF(timeRect.left() - s(18), rowRect.center().y() - s(6), s(12), s(12)), tokens.accent, true);
        }

        const int contentOffset = ContentOffset(option.font, index.row());
        const QRect contentRect(rowRect.left() + contentOffset, rowRect.top() + s(5),
                                ContentWidth(rowRect.width(), option.font, index.row(), favoriteMarker),
                                rowRect.height() - s(10));
        if (item.kind == ClipKind::Image) {
            DrawImage(painter, contentRect, item, historyIndex);
        } else {
            DrawText(painter, contentRect, item.text, option.font, primary);
        }

        if (!selected) {
            painter->setPen(QPen(WithAlpha(palette.color(QPalette::Text), 22), s(1)));
            painter->drawLine(rowRect.left(), rowRect.bottom(),
                              rowRect.right(), rowRect.bottom());
        }
        painter->restore();
    }

private:
    int s(int value) const { return Scaled(value, scaleFactor_); }

    static QFont NumberFont(QFont font) {
        font.setPointSizeF(qMax(8.0, font.pointSizeF() - 0.5));
        return font;
    }

    int ContentOffset(const QFont& font, int historyIndex) const {
        const int numberWidth = QFontMetrics(NumberFont(font)).horizontalAdvance(QString::number(historyIndex + 1));
        // Keep one quarter of the original gap, including for multi-digit labels.
        const int gap = qMax(s(2), qRound((s(kNumberWidth) - numberWidth) / 4.0));
        return s(2) + numberWidth + gap;
    }

    int ContentWidth(int rowWidth, const QFont& font, int historyIndex, bool favoriteMarker = false) const {
        return qMax(1, rowWidth - ContentOffset(font, historyIndex) - s(kTimeWidth + 8 + (favoriteMarker ? 18 : 0)));
    }

    static QString PreviewText(QString text) {
        // Only three lines are shown. Do not shape an entire multi-megabyte clip.
        constexpr qsizetype maxPreviewCharacters = 2048;
        if (text.size() > maxPreviewCharacters) {
            text.truncate(maxPreviewCharacters);
            if (text.back().isHighSurrogate()) text.chop(1);
            text += QChar(0x2026);
        }
        text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
        text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
        text.replace(QLatin1Char('\t'), QLatin1Char(' '));
        text.replace(QLatin1Char('\n'), QChar::LineSeparator);
        return text;
    }

    struct PreviewLines { int lines; bool truncated; };

    static PreviewLines LayoutPreview(QTextLayout& layout, int width) {
        QTextOption textOption;
        textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        layout.setTextOption(textOption);
        layout.beginLayout();
        int count = 0;
        const int lineHeight = QFontMetrics(layout.font()).height();
        for (; count < kMaxPreviewLines; ++count) {
            QTextLine line = layout.createLine();
            if (!line.isValid()) break;
            line.setLineWidth(qMax(1, width));
            line.setPosition(QPointF(0, count * lineHeight));
        }
        const bool truncated = layout.createLine().isValid();
        layout.endLayout();
        return {qMax(1, count), truncated};
    }

    void DrawImage(QPainter* painter, const QRect& rect, const ClipItem& item,
                   int /*historyIndex*/) const {
        // Keep thumbnails across tab switches and history reordering. Pin the
        // shared PNG bytes in each bounded entry so pointer identities cannot recycle.
        const QString key = QStringLiteral("%1:%2:%3").arg(reinterpret_cast<quintptr>(item.data.constData()))
            .arg(rect.width()).arg(rect.height());
        if (const auto* cached = thumbCache_.object(key)) {
            painter->drawPixmap(rect.topLeft(), cached->pixmap);
            return;
        }
        QImage image;
        if (!image.loadFromData(item.data)) {
            return;
        }
        const QPixmap pixmap = QPixmap::fromImage(
            image.scaled(rect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        if (pixmap.isNull()) {
            return;
        }
        const int cost = qMax<qsizetype>(1, (item.data.size() + pixmap.width() * pixmap.height() * 4 + 1023) / 1024);
        thumbCache_.insert(key, new Thumbnail{pixmap, item.data}, cost);
        painter->drawPixmap(rect.topLeft(), pixmap);
    }

    static void DrawText(QPainter* painter, const QRect& rect, const QString& text,
                         const QFont& font, const QColor& color) {
        painter->save();
        painter->setClipRect(rect, Qt::IntersectClip);
        painter->setFont(font);
        painter->setPen(color);

        QTextLayout layout(PreviewText(text), font);
        const PreviewLines preview = LayoutPreview(layout, rect.width());

        for (int i = 0; i < qMin(preview.lines, layout.lineCount()); ++i) {
            const QTextLine line = layout.lineAt(i);
            if (i == preview.lines - 1 && preview.truncated) {
                QString lastLine = layout.text().mid(line.textStart(), line.textLength());
                lastLine.remove(QChar::LineSeparator);
                const QString elided = QFontMetrics(font).elidedText(
                    lastLine.trimmed() + QChar(0x2026), Qt::ElideRight, rect.width());
                painter->drawText(rect.left(),
                                  rect.top() + qRound(line.y()) + QFontMetrics(font).ascent(),
                                  elided);
            } else {
                line.draw(painter, rect.topLeft());
            }
        }
        painter->restore();
    }

    struct Thumbnail {
        QPixmap pixmap;
        QByteArray source;
    };
    struct RowSize { int width; int row; QFont font; bool marked; QSize size; };
    struct ImageFingerprint { QByteArray source; QSize size; QByteArray digest; };

    ImageFingerprint Fingerprint(const ClipItem& item) {
        const auto key = reinterpret_cast<quintptr>(item.data.constData());
        if (const auto* cached = imageFingerprints_.object(key)) return *cached;
        const QImage image = QImage::fromData(item.data).convertToFormat(QImage::Format_ARGB32);
        ImageFingerprint result{item.data, image.size(), {}};
        if (!image.isNull()) result.digest = QCryptographicHash::hash(
            QByteArrayView(reinterpret_cast<const char*>(image.constBits()), image.sizeInBytes()), QCryptographicHash::Sha256);
        imageFingerprints_.insert(key, new ImageFingerprint(result), qMax<qsizetype>(1, (item.data.size() + 1023) / 1024));
        return result;
    }

    bool SameForMarker(const ClipItem& a, const ClipItem& b) {
        if (a.kind == ClipKind::Text) return a.text == b.text;
        if (a.pixelHash && b.pixelHash && a.width > 0 && a.height > 0 && b.width > 0 && b.height > 0)
            return a.width == b.width && a.height == b.height && a.pixelHash == b.pixelHash;
        if (a.data == b.data) return true;
        // Preserve legacy pixel equality without decoding both PNGs for every pair.
        const auto left = Fingerprint(a), right = Fingerprint(b);
        return !left.digest.isEmpty() && left.size == right.size && left.digest == right.digest;
    }

    std::vector<ClipItem> markerItems_;
    std::vector<bool> favoriteMarkers_;
    QCache<quintptr, ImageFingerprint> imageFingerprints_{32768}; // KiB, retained encoded bytes.
    const ClipboardHistory* history_ = nullptr;
    qreal scaleFactor_ = 1.0;
    int backgroundAlpha_ = 255;
    mutable QCache<QString, Thumbnail> thumbCache_{32768};
    mutable QHash<int, RowSize> sizeCache_;
};

} // namespace

HistoryWindow::HistoryWindow(qreal scaleFactor, QWidget* parent)
    : QWidget(parent)
    , scaleFactor_(qMax<qreal>(1.0, scaleFactor)) {
    setObjectName(QStringLiteral("HistoryWindowRoot"));
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setMinimumSize(scaled(kMinimumWidth), scaled(kMinimumHeight));
    resize(scaled(kPopupWidth), scaled(kPopupHeight));

    auto* root = new QVBoxLayout(this);
    // Keep one physical pixel around the children so the window-colored frame
    // is not covered by the header/body widgets.
    root->setContentsMargins(scaled(1), scaled(1), scaled(1), scaled(1));
    root->setSpacing(0);

    header_ = new QWidget(this);
    header_->setFixedHeight(scaled(kHeaderHeight));
    auto* headerLayout = new QHBoxLayout(header_);
    headerLayout->setContentsMargins(scaled(7), scaled(5), scaled(5), scaled(5));
    headerLayout->setSpacing(scaled(5));

    searchEdit_ = new QLineEdit(header_);
    searchEdit_->setObjectName(QStringLiteral("clipSearchEdit"));
    searchEdit_->setPlaceholderText(QStringLiteral("Search"));
    clearSearchButton_ = new QToolButton(searchEdit_);
    clearSearchButton_->setObjectName(QStringLiteral("clipSearchClearButton"));
    clearSearchButton_->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    clearSearchButton_->setIconSize(QSize(scaled(12), scaled(12)));
    clearSearchButton_->setFixedSize(scaled(18), scaled(18));
    clearSearchButton_->setToolTip(QCoreApplication::translate("App", "清空搜索"));
    clearSearchButton_->setAccessibleName(clearSearchButton_->toolTip());
    clearSearchButton_->setCursor(Qt::PointingHandCursor);
    clearSearchButton_->hide();
    auto* searchLayout = new QHBoxLayout(searchEdit_);
    searchLayout->setContentsMargins(0, 0, scaled(3), 0);
    searchLayout->addStretch();
    searchLayout->addWidget(clearSearchButton_, 0, Qt::AlignVCenter);
    searchEdit_->setTextMargins(0, 0, scaled(22), 0);
    connect(clearSearchButton_, &QToolButton::clicked, searchEdit_, &QLineEdit::clear);
    connect(searchEdit_, &QLineEdit::textChanged, this, [this](const QString& text) {
        clearSearchButton_->setVisible(!text.isEmpty());
    });
    searchEdit_->setFixedHeight(scaled(kSearchHeight));
    searchEdit_->setFixedWidth(scaled(kPopupWidth / 3));
    headerLayout->addWidget(searchEdit_);
    viewSwitcher_ = new QWidget(header_);
    viewSwitcher_->setObjectName(QStringLiteral("clipViewSwitcher"));
    auto* switchLayout = new QHBoxLayout(viewSwitcher_);
    switchLayout->setContentsMargins(scaled(1), scaled(1), scaled(1), scaled(1));
    switchLayout->setSpacing(scaled(1));
    viewGroup_ = new QButtonGroup(this);
    viewGroup_->setObjectName(QStringLiteral("clipViewGroup"));
    viewGroup_->setExclusive(true);
    auto makeViewButton = [&](int id, const QString& name, const QString& label) {
        auto* button = new QToolButton(viewSwitcher_);
        button->setObjectName(name);
        button->setCheckable(true);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setFixedSize(scaled(22), scaled(22));
        button->setIconSize(QSize(scaled(16), scaled(16)));
        button->setToolTip(label);
        button->setAccessibleName(label);
        button->setCursor(Qt::PointingHandCursor);
        viewGroup_->addButton(button, id);
        switchLayout->addWidget(button);
        return button;
    };
    clipboardViewButton_ = makeViewButton(0, QStringLiteral("clipHistoryButton"), QCoreApplication::translate("App", "剪切板"));
    favoritesViewButton_ = makeViewButton(1, QStringLiteral("clipFavoritesButton"), QCoreApplication::translate("App", "收藏"));
    clipboardViewButton_->setChecked(true);
    headerLayout->addWidget(viewSwitcher_);
    headerLayout->addStretch(1);
    connect(viewGroup_, &QButtonGroup::idClicked, this, [this](int id) {
        pages_->setCurrentIndex(id == 2 ? 1 : 0);
        searchEdit_->setEnabled(id != 2);
        if (id == 2) { syncPage_->setFocus(); return; }
        searchEdit_->setPlaceholderText(id == 1
            ? QCoreApplication::translate("App", "搜索收藏")
            : QCoreApplication::translate("App", "搜索剪切板"));
        BuildList();
    });
    searchEdit_->setPlaceholderText(QCoreApplication::translate("App", "搜索剪切板"));

    auto makeHeaderButton = [this](const QString& text,
                                   const QString& objectName) {
        auto* button = new QToolButton(header_);
        button->setObjectName(objectName);
        button->setText(text);
        button->setCursor(Qt::PointingHandCursor);
        button->setFixedSize(scaled(24), scaled(24));
        return button;
    };
    syncSettingsButton_ = makeHeaderButton({}, QStringLiteral("clipSyncSettingsButton"));
    syncSettingsButton_->setCheckable(true);
    syncSettingsButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    syncSettingsButton_->setIconSize(QSize(scaled(16), scaled(16)));
    syncSettingsButton_->setToolTip(QCoreApplication::translate("App", "云同步"));
    syncSettingsButton_->setAccessibleName(QCoreApplication::translate("App", "云同步"));
    viewGroup_->addButton(syncSettingsButton_, 2);
    headerLayout->addWidget(syncSettingsButton_);
    closeButton_ = makeHeaderButton(QStringLiteral("×"),
                                    QStringLiteral("clipCloseButton"));
    closeButton_->setToolTip(QCoreApplication::translate("App", "关闭"));
    closeButton_->setAccessibleName(closeButton_->toolTip());
    headerLayout->addWidget(closeButton_);
    root->addWidget(header_);

    body_ = new QWidget(this);
    auto* bodyLayout = new QVBoxLayout(body_);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    list_ = new QListWidget(body_);
    list_->setObjectName(QStringLiteral("clipList"));
    list_->setUniformItemSizes(false);
    list_->setResizeMode(QListView::Adjust);
    list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->setMouseTracking(false);
    list_->setFrameShape(QFrame::NoFrame);
    delegate_ = new HistoryDelegate(nullptr, scaleFactor_, list_);
    list_->setItemDelegate(delegate_);

    emptyState_ = new QWidget(body_);
    auto* emptyLayout = new QVBoxLayout(emptyState_);
    emptyLayout->setContentsMargins(scaled(32), scaled(20), scaled(32), scaled(20));
    emptyLayout->setSpacing(scaled(8));
    emptyLayout->setAlignment(Qt::AlignCenter);
    emptyTitle_ = new QLabel(QStringLiteral("Clipboard is empty"), emptyState_);
    emptyDescription_ = new QLabel(
        QStringLiteral("Copy text or an image and it will appear here automatically."),
        emptyState_);
    emptyTitle_->setAlignment(Qt::AlignCenter);
    emptyDescription_->setAlignment(Qt::AlignCenter);
    emptyDescription_->setWordWrap(true);
    emptyLayout->addWidget(emptyTitle_);
    emptyLayout->addWidget(emptyDescription_);

    bodyLayout->addWidget(list_, 1);
    bodyLayout->addWidget(emptyState_);
    pages_ = new QStackedWidget(this);
    pages_->setObjectName(QStringLiteral("clipPages"));
    pages_->addWidget(body_);
    syncPage_ = new QWidget(pages_);
    pages_->addWidget(syncPage_);
    root->addWidget(pages_, 1);

    toastLabel_ = new QLabel(this);
    toastLabel_->setAlignment(Qt::AlignCenter);
    toastLabel_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    toastLabel_->hide();

    resizeGrip_ = new QSizeGrip(this);
    resizeGrip_->setFixedSize(scaled(16), scaled(16));
    resizeGrip_->setCursor(Qt::SizeFDiagCursor);
    resizeGrip_->raise();

    timeTimer_ = new QTimer(this);
    timeTimer_->setInterval(60 * 1000);
    toastTimer_ = new QTimer(this);
    toastTimer_->setSingleShot(true);
    resizeSaveTimer_ = new QTimer(this);
    resizeSaveTimer_->setSingleShot(true);
    resizeSaveTimer_->setInterval(250);

    connect(closeButton_, &QToolButton::clicked, this, &HistoryWindow::onCloseClicked);
    connect(searchEdit_, &QLineEdit::textChanged, this, &HistoryWindow::onSearchChanged);
    connect(list_, &QListWidget::itemActivated, this, &HistoryWindow::onItemActivated);
    connect(timeTimer_, &QTimer::timeout, this, &HistoryWindow::onRelativeTimeTick);
    connect(toastTimer_, &QTimer::timeout, toastLabel_, &QLabel::hide);
    connect(resizeSaveTimer_, &QTimer::timeout, this, [this]() {
        if (resized_) {
            resized_(size());
        }
    });

    list_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(list_, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        if (QListWidgetItem* item = list_->itemAt(pos)) {
            list_->setCurrentItem(item);
            ShowContextMenu(list_->viewport()->mapToGlobal(pos));
        }
    });

    header_->installEventFilter(this);
    resizeGrip_->installEventFilter(this);
    list_->installEventFilter(this);
    searchEdit_->installEventFilter(this);
    QFont popupFont = font();
    Util::scaleFont(popupFont, scaleFactor_);
    setFont(popupFont);
    ApplyTheme();
}

int HistoryWindow::scaled(int value) const {
    return Scaled(value, scaleFactor_);
}

void HistoryWindow::Configure(const ClipboardHistory* history,
                              PasteCallback paste,
                              DeleteCallback remove,
                              CloseCallback close,
                              AiFillCallback aiFill,
                              MoveCallback moved,
                              ResizeCallback resized,
                              bool aiConfigured, DeleteCallback toggleFavorite) {
    history_ = history;
    paste_ = std::move(paste);
    remove_ = std::move(remove);
    toggleFavorite_ = std::move(toggleFavorite);
    close_ = std::move(close);
    aiFill_ = std::move(aiFill);
    moved_ = std::move(moved);
    resized_ = std::move(resized);
    aiConfigured_ = aiConfigured;
    static_cast<HistoryDelegate*>(delegate_)->setHistory(history_);
}

void HistoryWindow::SetSavedPosition(const QPoint& position, bool available) {
    savedPosition_ = position;
    hasSavedPosition_ = available;
}

void HistoryWindow::SetSavedSize(const QSize& size, bool available, qreal savedScaleFactor) {
    if (!available || !size.isValid()) {
        return;
    }
    const qreal currentScale = scaleFactor_;
    const qreal sourceScale = qMax<qreal>(1.0, savedScaleFactor);
    const QSize adjusted(qRound(size.width() * currentScale / sourceScale),
                         qRound(size.height() * currentScale / sourceScale));
    resize(qMax(scaled(kMinimumWidth), adjusted.width()),
           qMax(scaled(kMinimumHeight), adjusted.height()));
}

void HistoryWindow::ShowPopup(bool activate) {
    ApplyTheme();
    Refresh();
    QScreen* screen = hasSavedPosition_
                          ? QGuiApplication::screenAt(
                                QRect(savedPosition_, size()).center())
                          : QGuiApplication::screenAt(QCursor::pos());
    if (!screen) {
        screen = QApplication::primaryScreen();
    }
    if (screen) {
        const QRect available = screen->availableGeometry();
        resize(qMin(width(), available.width()),
               qMin(height(), available.height()));
        QPoint target = hasSavedPosition_
                            ? savedPosition_
                            : QPoint(available.right() - width() - scaled(8) + 1,
                                     available.top() + scaled(8));
        target.setX(qBound(available.left(), target.x(),
                           qMax(available.left(), available.right() - width() + 1)));
        target.setY(qBound(available.top(), target.y(),
                           qMax(available.top(), available.bottom() - height() + 1)));
        move(target);
    }
    show();
    raise();
    if (activate) {
        activateWindow();
    }
    if (viewGroup_->checkedId() == 2) syncPage_->setFocus();
    else searchEdit_->setFocus();
    timeTimer_->start();
}

void HistoryWindow::HidePopup() {
    hide();
}

void HistoryWindow::Refresh() {
    BuildList();
}

void HistoryWindow::ShowHint(const QString& text, int timeoutMs) {
    ShowToast(text, timeoutMs);
}

bool HistoryWindow::IsPopupVisible() const {
    return isVisible();
}

void HistoryWindow::onSearchChanged() {
    lastQuery_ = searchEdit_->text();
    BuildList();
}

void HistoryWindow::onItemActivated(QListWidgetItem*) {
    PasteSelected();
}

void HistoryWindow::onRelativeTimeTick() {
    list_->viewport()->update();
}

void HistoryWindow::onCloseClicked() {
    HidePopup();
    if (close_) {
        close_();
    }
}

size_t HistoryWindow::HistoryIndexForRow(int row) const {
    if (!list_ || row < 0 || row >= list_->count()) {
        return static_cast<size_t>(-1);
    }
    return static_cast<size_t>(list_->item(row)->data(Qt::UserRole).toInt());
}

void HistoryWindow::BuildList() {
    if (viewGroup_->checkedId() == 2) return; // Keep the hidden list intact on the sync page.
    auto* delegate = static_cast<HistoryDelegate*>(delegate_);
    if (history_) delegate->UpdateFavoriteMarkers();
    const QSignalBlocker blocker(list_);
    list_->setUpdatesEnabled(false);
    list_->clear();
    const bool favorites = viewGroup_->checkedId() == 1;
    const QStringList terms = QueryTerms(lastQuery_);
    const size_t count = history_ ? history_->Items().size() : 0;
    for (size_t i = 0; i < count; ++i) {
        const ClipItem& item = history_->Items()[i];
        if (item.favorite != favorites) continue;
        const QString searchable = item.kind == ClipKind::Text ? item.text : QString();
        if (!terms.isEmpty() && !MatchesAllTerms(searchable, terms)) {
            continue;
        }
        auto* row = new QListWidgetItem(searchable);
        row->setData(Qt::UserRole, static_cast<int>(i));
        const bool marked = !favorites && delegate->FavoriteMarked(i);
        row->setData(kFavoriteRole, marked);
        list_->addItem(row);
    }

    if (list_->count() == 0) {
        emptyTitle_->setText(!terms.isEmpty()
            ? QCoreApplication::translate("App", "没有匹配结果")
            : favorites ? QCoreApplication::translate("App", "暂无收藏")
                        : QCoreApplication::translate("App", "剪切板为空"));
        emptyDescription_->setText(!terms.isEmpty()
            ? QCoreApplication::translate("App", "试试其他关键词。")
            : favorites ? QCoreApplication::translate("App", "右键剪切板记录，选择收藏即可保留。")
                        : QCoreApplication::translate("App", "复制文字或图片后会自动显示在这里。"));
        list_->hide();
        emptyState_->show();
    } else {
        emptyState_->hide();
        list_->show();
        list_->setCurrentRow(0);
    }
    list_->setUpdatesEnabled(true);
}

void HistoryWindow::PasteSelected() {
    PasteRow(list_->currentRow());
}

void HistoryWindow::PasteRow(int row) {
    const size_t index = HistoryIndexForRow(row);
    if (index != static_cast<size_t>(-1) && paste_) {
        paste_(index);
    }
}

void HistoryWindow::CopySelected() {
    const size_t index = HistoryIndexForRow(list_->currentRow());
    if (!history_ || index == static_cast<size_t>(-1) || index >= history_->Items().size()) {
        return;
    }
    const ClipItem& item = history_->Items()[index];
    if (item.kind == ClipKind::Text) {
        QApplication::clipboard()->setText(item.text);
    } else {
        QImage image;
        if (!image.loadFromData(item.data)) {
            return;
        }
        QApplication::clipboard()->setImage(image);
    }
}

void HistoryWindow::DeleteSelected() {
    const size_t index = HistoryIndexForRow(list_->currentRow());
    if (index != static_cast<size_t>(-1) && remove_) {
        remove_(index);
    }
}

void HistoryWindow::AiFillSelected() {
    const size_t index = HistoryIndexForRow(list_->currentRow());
    if (index != static_cast<size_t>(-1) && aiFill_) {
        aiFill_(index);
    }
}

void HistoryWindow::ShowContextMenu(const QPoint& globalPos) {
    const size_t index = HistoryIndexForRow(list_->currentRow());
    if (!history_ || index == static_cast<size_t>(-1) || index >= history_->Items().size()) {
        return;
    }

    const ClipItem contextItem = history_->Items()[index];
    const bool isText = contextItem.kind == ClipKind::Text;
    QMenu menu(this);
    QAction* favoriteAction = menu.addAction(contextItem.favorite
        ? QCoreApplication::translate("App", "取消收藏")
        : QCoreApplication::translate("App", "收藏"));
    favoriteAction->setIcon(QIcon(ViewIcon(true, scaled(16)).pixmap(QSize(scaled(16), scaled(16)),
        QIcon::Normal, contextItem.favorite ? QIcon::On : QIcon::Off)));
    favoriteAction->setEnabled(static_cast<bool>(toggleFavorite_));
    QAction* copyAction = menu.addAction(QCoreApplication::translate("App", "复制"));
    QAction* previewAction = nullptr;
    if (contextItem.kind == ClipKind::Image) {
        previewAction = menu.addAction(QCoreApplication::translate("App", "预览"));
    }
    QAction* deleteAction = menu.addAction(QCoreApplication::translate("App", "删除"));
    menu.addSeparator();
    QAction* aiAction = menu.addAction(QCoreApplication::translate("App", "AI 填充"));
    aiAction->setEnabled(isText && aiConfigured_ && aiFill_);
    QAction* selected = menu.exec(globalPos);
    if (selected == favoriteAction && toggleFavorite_) {
        const auto& items = history_->Items();
        const auto found = std::find_if(items.begin(), items.end(), [&](const ClipItem& item) {
            return item.favorite == contextItem.favorite && ClipboardHistory::SameContent(item, contextItem);
        });
        if (found != items.end()) toggleFavorite_(static_cast<size_t>(std::distance(items.begin(), found)));
    } else if (previewAction && selected == previewAction) {
        const QPixmap pixmap = QPixmap::fromImage(QImage::fromData(contextItem.data, "PNG"));
        if (!pixmap.isNull()) {
            const QScreen* historyScreen = screen();
            const QRect previewArea = historyScreen ? historyScreen->availableGeometry() : frameGeometry();
            const QPoint screenCenter = previewArea.topLeft()
                + QPoint(previewArea.width() / 2, previewArea.height() / 2);
            emit imagePreviewRequested(pixmap, screenCenter);
        } else {
            ShowHint(QCoreApplication::translate("App", "无法预览此图片"));
        }
    } else if (selected == copyAction) {
        CopySelected();
    } else if (selected == deleteAction) {
        DeleteSelected();
    } else if (selected == aiAction) {
        AiFillSelected();
    }
}

void HistoryWindow::ApplyTheme() {
    const QPalette palette = qApp->palette();
    const ThemeTokens& tokens = ThemeManager::tokens();
    const QColor base = palette.color(QPalette::Base);
    const QColor text = palette.color(QPalette::Text);
    const QColor muted = palette.color(QPalette::PlaceholderText);
    const QColor border = WithAlpha(text, 28);
    const QColor listText = tokens.textPrimary;
    const QColor listMuted = tokens.textSecondary;

    QPalette listPalette = list_->palette();
    listPalette.setColor(QPalette::Base, Qt::transparent);
    listPalette.setColor(QPalette::AlternateBase, Qt::transparent);
    listPalette.setColor(QPalette::Text, listText);
    listPalette.setColor(QPalette::PlaceholderText, listMuted);
    list_->setPalette(listPalette);

    header_->setStyleSheet(QStringLiteral("background: transparent; border-bottom: %1px solid %2;")
                               .arg(scaled(1)).arg(CssColor(border)));
    body_->setStyleSheet(QStringLiteral("background: transparent;"));
    clipboardViewButton_->setIcon(ViewIcon(false, scaled(16)));
    favoritesViewButton_->setIcon(ViewIcon(true, scaled(16)));
    viewSwitcher_->setStyleSheet(QStringLiteral(
        "QWidget#clipViewSwitcher { background: %1; border: %2px solid %3; border-radius: %4px; }"
        "QToolButton { background: transparent; border: none; border-radius: %5px; padding: 0; }"
        "QToolButton:hover { background: %6; }"
        "QToolButton:checked { background: %7; }"
        "QToolButton:focus { border: %2px solid %8; }")
        .arg(CssColor(base)).arg(scaled(1)).arg(CssColor(border)).arg(scaled(5))
        .arg(scaled(4)).arg(CssColor(WithAlpha(text, 20)), CssColor(tokens.accentSubtle), CssColor(tokens.accent)));
    emptyTitle_->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                   .arg(CssColor(text)));
    emptyDescription_->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                         .arg(CssColor(muted)));
    searchEdit_->setStyleSheet(
        QStringLiteral("QLineEdit { color: %1; background: %2; border: %3px solid %4;"
                       " border-radius: %5px; padding: 0 %6px; }"
                       "QLineEdit:focus { border-color: %7; }")
            .arg(CssColor(text), CssColor(base))
            .arg(scaled(1)).arg(CssColor(border)).arg(scaled(5)).arg(scaled(7))
            .arg(CssColor(palette.color(QPalette::Highlight))));
    list_->setStyleSheet(
        QStringLiteral("QListWidget { background: %1; border: none; padding: 0; }"
                       "QListWidget::item { background: transparent; border: none; margin: 0; padding: 0; }"
                       "QScrollBar:vertical { background: %1; width: %2px; }"
                       "QScrollBar::handle:vertical { background: %3; border-radius: %4px;"
                       " min-height: %5px; }"
                       "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
            .arg(QStringLiteral("transparent")).arg(scaled(7))
            .arg(CssColor(WithAlpha(listMuted, 120))).arg(scaled(4)).arg(scaled(24)));
    const QString buttonStyle =
        QStringLiteral("QToolButton { color: %1; background: transparent; border: none;"
                       " padding: 0px; margin: 0px; border-radius: %4px; font-size: %5px; }"
                       "QToolButton:hover { background: %2; color: %3; }"
                       "QToolButton:pressed { background: %6; }")
            .arg(CssColor(muted), CssColor(WithAlpha(text, 20)), CssColor(text))
            .arg(scaled(4)).arg(scaled(14)).arg(CssColor(WithAlpha(text, 40)));
    closeButton_->setStyleSheet(buttonStyle);
    clearSearchButton_->setStyleSheet(buttonStyle);
    syncSettingsButton_->setIcon(CloudIcon(scaled(16)));
    syncSettingsButton_->setStyleSheet(buttonStyle + QStringLiteral("QToolButton:checked { background: %1; } QToolButton:focus { border: 1px solid %2; }").arg(CssColor(tokens.accentSubtle), CssColor(tokens.accent)));
    toastLabel_->setStyleSheet(
        QStringLiteral("color: %1; background: %2; padding: %3px %4px; border-radius: %5px;")
            .arg(CssColor(palette.color(QPalette::HighlightedText)),
                 CssColor(palette.color(QPalette::Highlight)))
            .arg(scaled(5)).arg(scaled(12)).arg(scaled(10)));
    resizeGrip_->setStyleSheet(QStringLiteral("background: transparent;"));
    update();
    list_->viewport()->update();
}

void HistoryWindow::SetSyncStatus(const QString& status) {
    syncSettingsButton_->setToolTip(QCoreApplication::translate("App", "云同步") + QStringLiteral("\n") + status);
}

void HistoryWindow::SetSyncPage(QWidget* page) {
    if (!page || page == syncPage_) return;
    const bool active = pages_->currentWidget() == syncPage_;
    pages_->removeWidget(syncPage_);
    delete syncPage_;
    syncPage_ = page;
    pages_->addWidget(page);
    if (active) pages_->setCurrentWidget(page);
}

void HistoryWindow::ShowSyncPage() {
    syncSettingsButton_->click();
}

void HistoryWindow::ShowToast(const QString& text, int timeoutMs) {
    if (text.isEmpty()) {
        return;
    }
    toastLabel_->setText(text);
    toastLabel_->adjustSize();
    toastLabel_->move(width() - toastLabel_->width() - scaled(8), scaled(kHeaderHeight + 6));
    toastLabel_->show();
    toastLabel_->raise();
    toastTimer_->start(timeoutMs);
}

void HistoryWindow::SetBackgroundTransparency(int percent) {
    backgroundAlpha_ = qRound(255 * (100 - qBound(0, percent, 100)) / 100.0);
    static_cast<HistoryDelegate*>(delegate_)->setBackgroundAlpha(backgroundAlpha_);
    update();
    list_->viewport()->update();
}

void HistoryWindow::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path;
    path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                        scaled(kRadius), scaled(kRadius));
    const QColor windowBackground = WithAlpha(palette().color(QPalette::Window), backgroundAlpha_);
    painter.fillPath(path, windowBackground);
    // Paint the list background once on the translucent top-level surface.
    // Replacing the header fill avoids stacking two translucent backgrounds.
    painter.save();
    painter.setClipPath(path);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(pages_->geometry(), WithAlpha(ThemeManager::tokens().surface, backgroundAlpha_));
    painter.restore();
    // Match the frame to the title area's effective background color. The
    // header is transparent and therefore uses QPalette::Window as well.
    painter.setPen(QPen(windowBackground, scaled(1)));
    painter.drawPath(path);
}

void HistoryWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        onCloseClicked();
        return;
    }
    QWidget::keyPressEvent(event);
}

void HistoryWindow::closeEvent(QCloseEvent* event) {
    timeTimer_->stop();
    toastTimer_->stop();
    QWidget::closeEvent(event);
}

void HistoryWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    timeTimer_->start();
}

void HistoryWindow::hideEvent(QHideEvent* event) {
    timeTimer_->stop();
    toastTimer_->stop();
    toastLabel_->hide();
    QWidget::hideEvent(event);
}

void HistoryWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::PaletteChange ||
        event->type() == QEvent::ApplicationPaletteChange) {
        ApplyTheme();
    }
    QWidget::changeEvent(event);
}

void HistoryWindow::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (resizeGrip_) {
        resizeGrip_->move(width() - resizeGrip_->width(),
                          height() - resizeGrip_->height());
        resizeGrip_->raise();
    }
    if (searchEdit_) {
        searchEdit_->setFixedWidth(qMax(scaled(80), width() / 3));
    }
    if (isVisible() && resizeSaveTimer_) {
        resizeSaveTimer_->start();
    }
}

bool HistoryWindow::eventFilter(QObject* obj, QEvent* event) {
    if (obj == resizeGrip_ && event->type() == QEvent::MouseButtonRelease) {
        resizeSaveTimer_->stop();
        if (resized_) {
            resized_(size());
        }
    }
    if (obj == header_) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->button() == Qt::LeftButton) {
                dragging_ = true;
                dragStartPos_ = mouse->globalPosition().toPoint();
            }
        } else if (event->type() == QEvent::MouseMove && dragging_) {
            auto* mouse = static_cast<QMouseEvent*>(event);
            const QPoint current = mouse->globalPosition().toPoint();
            move(pos() + current - dragStartPos_);
            dragStartPos_ = current;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            dragging_ = false;
            savedPosition_ = pos();
            hasSavedPosition_ = true;
            if (moved_) {
                moved_(savedPosition_);
            }
        }
    }

    if (event->type() != QEvent::KeyPress) {
        return QWidget::eventFilter(obj, event);
    }

    auto* key = static_cast<QKeyEvent*>(event);
    const bool fromSearch = obj == searchEdit_;
    const bool control = key->modifiers().testFlag(Qt::ControlModifier);
    if (key->key() == Qt::Key_Escape) {
        if (fromSearch && !searchEdit_->text().isEmpty()) {
            searchEdit_->clear();
        } else {
            onCloseClicked();
        }
        return true;
    }
    if (viewGroup_->checkedId() == 2) {
        return QWidget::eventFilter(obj, event);
    }
    if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
        PasteSelected();
        return true;
    }
    if (control && key->key() == Qt::Key_F) {
        searchEdit_->setFocus();
        searchEdit_->selectAll();
        return true;
    }
    if (fromSearch) {
        if (key->key() == Qt::Key_Down) {
            list_->setFocus();
            return true;
        }
        return QWidget::eventFilter(obj, event);
    }
    if (control && key->key() == Qt::Key_C) {
        CopySelected();
        return true;
    }
    if (key->key() == Qt::Key_Delete) {
        DeleteSelected();
        return true;
    }
    if (key->key() == Qt::Key_I && aiConfigured_) {
        AiFillSelected();
        return true;
    }
    if (key->key() >= Qt::Key_1 && key->key() <= Qt::Key_9) {
        PasteRow(key->key() - Qt::Key_1);
        return true;
    }
    return QWidget::eventFilter(obj, event);
}
