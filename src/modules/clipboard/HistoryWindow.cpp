#include "HistoryWindow.h"

#include "core/foundation/TimeUtil.h"
#include "core/platform/Util.h"
#include "core/theme/ThemeIcon.h"
#include "core/theme/ThemeManager.h"

#include <QApplication>
#include <QClipboard>
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
#include <QVBoxLayout>

namespace {

constexpr int kHeaderHeight = 34;
constexpr int kSearchHeight = 24;
constexpr int kRowHeight = 58;
constexpr int kNumberWidth = 28;
constexpr int kTimeWidth = 40;

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
        history_ = history;
    }

    void ClearThumbnails() {
        thumbCache_.clear();
    }

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override {
        return QSize(s(HistoryWindow::kPopupWidth), s(kRowHeight));
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
                              tokens.accentSubtle);
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

        QFont numberFont = option.font;
        numberFont.setPointSizeF(qMax(8.0, numberFont.pointSizeF() - 0.5));
        painter->setFont(numberFont);
        painter->setPen(secondary);
        const QRect numberRect(rowRect.left() + s(3), rowRect.top() + s(6),
                               s(kNumberWidth - 6), rowRect.height() - s(12));
        painter->drawText(numberRect, Qt::AlignTop | Qt::AlignRight,
                          QString::number(historyIndex + 1));

        const QRect timeRect(rowRect.right() - s(kTimeWidth), rowRect.top() + s(5),
                             s(kTimeWidth - 6), rowRect.height() - s(10));
        painter->setPen(secondary);
        painter->drawText(timeRect, Qt::AlignBottom | Qt::AlignRight,
                          TimeUtil::FormatRelativeTime(item.capturedAt));

        const QRect contentRect(rowRect.left() + s(kNumberWidth + 2), rowRect.top() + s(5),
                                rowRect.width() - s(kNumberWidth + kTimeWidth + 10),
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

    void DrawImage(QPainter* painter, const QRect& rect, const ClipItem& item,
                   int historyIndex) const {
        // Decode and downscale each image only once, then cache the thumbnail per
        // history item so scrolling repaints pixmaps instead of re-decoding the
        // full-resolution PNG (and smooth-scaling it) on every frame. The cache is
        // cleared whenever the list is rebuilt, which is when indices can change.
        const auto cached = thumbCache_.constFind(historyIndex);
        if (cached != thumbCache_.constEnd() && cached->size == rect.size()) {
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
        thumbCache_.insert(historyIndex, {pixmap, rect.size()});
        painter->drawPixmap(rect.topLeft(), pixmap);
    }

    static void DrawText(QPainter* painter, const QRect& rect, QString text,
                         const QFont& font, const QColor& color) {
        text.replace(QRegularExpression(QStringLiteral("[\\r\\n\\t]+")), QStringLiteral(" "));
        painter->setFont(font);
        painter->setPen(color);

        QTextLayout layout(text, font);
        layout.beginLayout();
        QList<QTextLine> lines;
        for (int i = 0; i < 3; ++i) {
            QTextLine line = layout.createLine();
            if (!line.isValid()) {
                break;
            }
            line.setLineWidth(rect.width());
            line.setPosition(QPointF(0, i * line.height()));
            lines.append(line);
        }
        const bool truncated = layout.createLine().isValid();
        layout.endLayout();

        for (int i = 0; i < lines.size(); ++i) {
            const QTextLine& line = lines[i];
            if (i == lines.size() - 1 && truncated) {
                const QString remaining = text.mid(line.textStart());
                const QString elided = QFontMetrics(font).elidedText(
                    remaining, Qt::ElideRight, rect.width());
                painter->drawText(rect.left(),
                                  rect.top() + qRound(line.y()) + QFontMetrics(font).ascent(),
                                  elided);
            } else {
                line.draw(painter, rect.topLeft());
            }
        }
    }

    struct Thumbnail {
        QPixmap pixmap;
        QSize size;
    };

    const ClipboardHistory* history_ = nullptr;
    qreal scaleFactor_ = 1.0;
    mutable QHash<int, Thumbnail> thumbCache_;
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
    searchEdit_->setClearButtonEnabled(true);
    searchEdit_->setFixedHeight(scaled(kSearchHeight));
    searchEdit_->setFixedWidth(scaled(kPopupWidth / 3));
    headerLayout->addWidget(searchEdit_);
    headerLayout->addStretch(1);

    dragHandle_ = new QLabel(QStringLiteral("≡"), header_);
    dragHandle_->setAlignment(Qt::AlignCenter);
    dragHandle_->setCursor(Qt::SizeAllCursor);
    dragHandle_->setFixedSize(scaled(24), scaled(24));
    dragHandle_->setToolTip(QStringLiteral("Drag to move"));
    dragHandle_->move((width() - dragHandle_->width()) / 2,
                      (scaled(kHeaderHeight) - dragHandle_->height()) / 2);
    dragHandle_->raise();

    auto makeHeaderButton = [this](const QString& text, const QString& tooltip,
                                   const QString& objectName) {
        auto* button = new QToolButton(header_);
        button->setObjectName(objectName);
        button->setText(text);
        button->setToolTip(tooltip);
        button->setCursor(Qt::PointingHandCursor);
        button->setFixedSize(scaled(24), scaled(24));
        return button;
    };
    closeButton_ = makeHeaderButton(QStringLiteral("×"), QStringLiteral("Close"),
                                    QStringLiteral("clipCloseButton"));
    headerLayout->addWidget(closeButton_);
    root->addWidget(header_);

    body_ = new QWidget(this);
    auto* bodyLayout = new QVBoxLayout(body_);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    list_ = new QListWidget(body_);
    list_->setObjectName(QStringLiteral("clipList"));
    list_->setUniformItemSizes(true);
    list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->setMouseTracking(false);
    list_->setFrameShape(QFrame::NoFrame);
    delegate_ = new HistoryDelegate(nullptr, scaleFactor_, this);
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
    root->addWidget(body_, 1);

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
    dragHandle_->installEventFilter(this);
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
                              bool aiConfigured) {
    history_ = history;
    paste_ = std::move(paste);
    remove_ = std::move(remove);
    close_ = std::move(close);
    aiFill_ = std::move(aiFill);
    moved_ = std::move(moved);
    resized_ = std::move(resized);
    aiConfigured_ = aiConfigured;
    static_cast<HistoryDelegate*>(delegate_)->setHistory(history_);
    static_cast<HistoryDelegate*>(delegate_)->ClearThumbnails();
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
    searchEdit_->setFocus();
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
    list_->clear();
    static_cast<HistoryDelegate*>(delegate_)->ClearThumbnails();
    if (!history_ || history_->Empty()) {
        emptyTitle_->setText(QStringLiteral("Clipboard is empty"));
        emptyDescription_->setText(
            QStringLiteral("Copy text or an image and it will appear here automatically."));
        list_->hide();
        emptyState_->show();
        return;
    }

    const QStringList terms = QueryTerms(lastQuery_);
    for (size_t i = 0; i < history_->Items().size(); ++i) {
        const ClipItem& item = history_->Items()[i];
        const QString searchable = item.kind == ClipKind::Text ? item.text : QString();
        if (!terms.isEmpty() && !MatchesAllTerms(searchable, terms)) {
            continue;
        }
        auto* row = new QListWidgetItem(searchable, list_);
        row->setData(Qt::UserRole, static_cast<int>(i));
    }

    if (list_->count() == 0) {
        emptyTitle_->setText(QStringLiteral("No matches"));
        emptyDescription_->setText(QStringLiteral("Try a different search."));
        list_->hide();
        emptyState_->show();
    } else {
        emptyState_->hide();
        list_->show();
        list_->setCurrentRow(0);
    }
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

    const bool isText = history_->Items()[index].kind == ClipKind::Text;
    QMenu menu(this);
    QAction* copyAction = menu.addAction(QStringLiteral("Copy"));
    QAction* aiAction = menu.addAction(QStringLiteral("AI Fill"));
    aiAction->setEnabled(isText && aiConfigured_ && aiFill_);
    menu.addSeparator();
    QAction* deleteAction = menu.addAction(QStringLiteral("Delete"));
    QAction* selected = menu.exec(globalPos);
    if (selected == copyAction) {
        CopySelected();
    } else if (selected == aiAction) {
        AiFillSelected();
    } else if (selected == deleteAction) {
        DeleteSelected();
    }
}

void HistoryWindow::ApplyTheme() {
    const QPalette palette = qApp->palette();
    const ThemeTokens& tokens = ThemeManager::tokens();
    const QColor base = palette.color(QPalette::Base);
    const QColor text = palette.color(QPalette::Text);
    const QColor muted = palette.color(QPalette::PlaceholderText);
    const QColor border = WithAlpha(text, 28);
    const QColor listBase = tokens.surface;
    const QColor listText = tokens.textPrimary;
    const QColor listMuted = tokens.textSecondary;

    QPalette listPalette = list_->palette();
    listPalette.setColor(QPalette::Base, listBase);
    listPalette.setColor(QPalette::AlternateBase, listBase);
    listPalette.setColor(QPalette::Text, listText);
    listPalette.setColor(QPalette::PlaceholderText, listMuted);
    list_->setPalette(listPalette);

    header_->setStyleSheet(QStringLiteral("background: transparent; border-bottom: %1px solid %2;")
                               .arg(scaled(1)).arg(CssColor(border)));
    body_->setStyleSheet(QStringLiteral("background: transparent;"));
    emptyTitle_->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                   .arg(CssColor(text)));
    emptyDescription_->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                         .arg(CssColor(muted)));
    dragHandle_->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
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
            .arg(CssColor(listBase)).arg(scaled(7))
            .arg(CssColor(WithAlpha(listMuted, 120))).arg(scaled(4)).arg(scaled(24)));
    const QString buttonStyle =
        QStringLiteral("QToolButton { color: %1; background: transparent; border: none;"
                       " padding: 0px; margin: 0px; border-radius: %4px; font-size: %5px; }"
                       "QToolButton:hover { background: %2; color: %3; }"
                       "QToolButton:pressed { background: %6; }")
            .arg(CssColor(muted), CssColor(WithAlpha(text, 20)), CssColor(text))
            .arg(scaled(4)).arg(scaled(14)).arg(CssColor(WithAlpha(text, 40)));
    closeButton_->setStyleSheet(buttonStyle);
    toastLabel_->setStyleSheet(
        QStringLiteral("color: %1; background: %2; padding: %3px %4px; border-radius: %5px;")
            .arg(CssColor(palette.color(QPalette::HighlightedText)),
                 CssColor(palette.color(QPalette::Highlight)))
            .arg(scaled(5)).arg(scaled(12)).arg(scaled(10)));
    resizeGrip_->setStyleSheet(QStringLiteral("background: transparent;"));
    update();
    list_->viewport()->update();
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

void HistoryWindow::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path;
    path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                        scaled(kRadius), scaled(kRadius));
    painter.fillPath(path, palette().color(QPalette::Window));
    // Match the frame to the title area's effective background color. The
    // header is transparent and therefore uses QPalette::Window as well.
    painter.setPen(QPen(palette().color(QPalette::Window), scaled(1)));
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
    if (dragHandle_) {
        dragHandle_->move((width() - dragHandle_->width()) / 2,
                          (scaled(kHeaderHeight) - dragHandle_->height()) / 2);
        dragHandle_->raise();
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
    if (obj == header_ || obj == dragHandle_) {
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
