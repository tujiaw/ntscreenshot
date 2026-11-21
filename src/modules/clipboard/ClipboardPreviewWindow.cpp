#include "ClipboardPreviewWindow.h"

#include <QCoreApplication>
#include <QApplication>
#include <QFrame>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <cmath>

namespace {
QIcon PreviewActionIcon(int action, int size, const QColor& color) {
    const qreal dpr = qApp->devicePixelRatio();
    QPixmap pixmap(qRound(size * dpr), qRound(size * dpr));
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(size / 16.0, size / 16.0);
    painter.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    QPainterPath path;
    if (action == 0) {
        path.moveTo(7, 3); path.lineTo(2, 8); path.lineTo(7, 13);
        path.moveTo(2, 8); path.lineTo(14, 8);
    } else if (action == 1) {
        path.moveTo(3, 10); path.lineTo(10.5, 2.5);
        path.quadTo(11.5, 1.5, 12.5, 2.5);
        path.lineTo(13.5, 3.5); path.quadTo(14.5, 4.5, 13.5, 5.5);
        path.lineTo(6, 13); path.lineTo(2, 14); path.closeSubpath();
        path.moveTo(9, 4); path.lineTo(12, 7);
    } else {
        path.moveTo(3, 8); path.lineTo(6.5, 11.5); path.lineTo(13, 4.5);
    }
    painter.drawPath(path);
    painter.end();
    return QIcon(pixmap);
}

class PreviewImageView final : public QGraphicsView {
public:
    explicit PreviewImageView(QWidget* parent) : QGraphicsView(parent) {
        setScene(new QGraphicsScene(this));
        setFrameShape(QFrame::NoFrame);
        setDragMode(QGraphicsView::ScrollHandDrag);
        setRenderHint(QPainter::SmoothPixmapTransform);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    }
    void SetImage(const QPixmap& pixmap) {
        scene()->clear();
        scene()->addPixmap(pixmap);
        scene()->setSceneRect(pixmap.rect());
        autoFit_ = true;
        Fit();
    }
protected:
    void resizeEvent(QResizeEvent* event) override {
        QGraphicsView::resizeEvent(event);
        if (autoFit_) Fit();
    }
    void wheelEvent(QWheelEvent* event) override {
        if (event->angleDelta().y() == 0) { event->ignore(); return; }
        const qreal factor = std::pow(1.15, event->angleDelta().y() / 120.0);
        const qreal target = qBound(0.01, transform().m11() * factor, 32.0);
        scale(target / transform().m11(), target / transform().m11());
        autoFit_ = false;
        event->accept();
    }
private:
    void Fit() {
        resetTransform();
        if (!scene()->sceneRect().isEmpty()) fitInView(scene()->sceneRect(), Qt::KeepAspectRatio);
    }
    bool autoFit_ = true;
};
}

ClipboardPreviewWindow::ClipboardPreviewWindow(QWidget* parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("clipboardPreviewWindow"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFocusPolicy(Qt::StrongFocus);
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(4);

    auto* toolbar = new QWidget(this);
    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);
    toolbarLayout->setSpacing(4);
    backButton_ = new QToolButton(toolbar);
    backButton_->setObjectName(QStringLiteral("clipPreviewBackButton"));
    backButton_->setText(QCoreApplication::translate("App", "返回"));
    backButton_->setAccessibleName(QCoreApplication::translate("App", "返回"));
    backButton_->setCursor(Qt::PointingHandCursor);
    backButton_->setAutoRaise(true);
    backButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    backButton_->setToolTip(backButton_->text());
    connect(backButton_, &QToolButton::clicked, this, &ClipboardPreviewWindow::onBackClicked);
    editButton_ = new QToolButton(toolbar);
    editButton_->setObjectName(QStringLiteral("clipPreviewEditButton"));
    editButton_->setText(QCoreApplication::translate("App", "编辑"));
    editButton_->setAccessibleName(QCoreApplication::translate("App", "编辑"));
    editButton_->setCursor(Qt::PointingHandCursor);
    editButton_->setAutoRaise(true);
    editButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    editButton_->setToolTip(editButton_->text());
    connect(editButton_, &QToolButton::clicked, this, &ClipboardPreviewWindow::onEditClicked);
    toolbarLayout->addWidget(backButton_);
    toolbarLayout->addStretch(1);
    toolbarLayout->addWidget(editButton_);
    root->addWidget(toolbar);

    auto* separator = new QFrame(this);
    separator->setObjectName(QStringLiteral("clipPreviewSeparator"));
    separator->setFixedHeight(1);
    root->addWidget(separator);

    pages_ = new QStackedWidget(this);
    root->addWidget(pages_, 1);
    text_ = new QPlainTextEdit(pages_);
    text_->setObjectName(QStringLiteral("clipboardPreviewText"));
    text_->setFrameShape(QFrame::NoFrame);
    text_->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    text_->setTabChangesFocus(false);
    text_->setReadOnly(true);
    connect(text_, &QPlainTextEdit::textChanged, this, [this]() {
        if (!loading_) textDirty_ = true;
    });
    pages_->addWidget(text_);
    image_ = new PreviewImageView(pages_);
    image_->setObjectName(QStringLiteral("clipboardPreviewImage"));
    pages_->addWidget(image_);
    error_ = new QLabel(QCoreApplication::translate("App", "无法预览此图片"), pages_);
    error_->setAlignment(Qt::AlignCenter);
    pages_->addWidget(error_);
    // Keep Space/Escape and arrow navigation working even when the user clicks
    // into the preview content.
    text_->installEventFilter(this);
    text_->viewport()->installEventFilter(this);
    image_->installEventFilter(this);
    image_->viewport()->installEventFilter(this);
    error_->installEventFilter(this);
    UpdateIcons(16);
}

void ClipboardPreviewWindow::UpdateIcons(int size) {
    const QColor color = palette().color(QPalette::WindowText);
    backButton_->setIconSize(QSize(size, size));
    editButton_->setIconSize(QSize(size, size));
    backButton_->setIcon(PreviewActionIcon(0, size, color));
    editButton_->setIcon(PreviewActionIcon(editing_ ? 2 : 1, size, color));
}

void ClipboardPreviewWindow::ShowItem(const ClipItem& item) {
    const bool same = hasContent_ && current_.kind == item.kind
        && current_.text == item.text && current_.data == item.data;
    // Always leave edit mode, even when the content is unchanged; otherwise a
    // reopened item would stay editable and the edit button would mean "save".
    SetEditing(false);
    if (same) {
        return;
    }
    current_ = item;
    hasContent_ = true;
    loading_ = true;
    if (item.kind == ClipKind::Text) {
        static_cast<PreviewImageView*>(image_)->SetImage({});
        text_->setPlainText(item.text);
        pages_->setCurrentWidget(text_);
        editButton_->setVisible(true);
    } else {
        text_->clear();
        editButton_->setVisible(false);
        const QPixmap pixmap = QPixmap::fromImage(QImage::fromData(item.data));
        static_cast<PreviewImageView*>(image_)->SetImage(pixmap);
        pages_->setCurrentWidget(pixmap.isNull() ? static_cast<QWidget*>(error_) : image_);
    }
    loading_ = false;
    textDirty_ = false;
}

void ClipboardPreviewWindow::FocusImage() {
    if (pages_->currentWidget() == image_) image_->setFocus();
}

bool ClipboardPreviewWindow::CommitEdit() {
    if (!textDirty_ || !hasContent_ || current_.kind != ClipKind::Text) {
        return false;
    }
    textDirty_ = false;
    const QString edited = text_->toPlainText();
    if (edited == current_.text) {
        return false;
    }
    current_.text = edited;
    emit editCommitted(edited);
    return true;
}

void ClipboardPreviewWindow::SetEditing(bool editing) {
    editing_ = editing;
    text_->setReadOnly(!editing);
    const QString label = editing ? QCoreApplication::translate("App", "保存")
                                  : QCoreApplication::translate("App", "编辑");
    editButton_->setText(label);
    editButton_->setAccessibleName(label);
    editButton_->setToolTip(label);
    UpdateIcons(editButton_->iconSize().width());
    if (editing) {
        text_->setFocus();
    }
}

void ClipboardPreviewWindow::onEditClicked() {
    if (editing_) {
        // Save and return to the list.
        CommitEdit();
        emit dismissRequested();
    } else {
        SetEditing(true);
    }
}

void ClipboardPreviewWindow::onBackClicked() {
    // The edit is committed by HistoryWindow on dismiss; just close the pane.
    emit dismissRequested();
}

void ClipboardPreviewWindow::keyPressEvent(QKeyEvent* event) {
    // When the pane itself (not the text editor) has focus, Space/Escape toggle
    // it closed; the editor keeps Space for typing and handles Escape separately.
    if (event->modifiers() == Qt::NoModifier
        && (event->key() == Qt::Key_Escape || event->key() == Qt::Key_Space)) {
        emit dismissRequested();
        return;
    }
    QWidget::keyPressEvent(event);
}

bool ClipboardPreviewWindow::eventFilter(QObject* object, QEvent* event) {
    if (event->type() == QEvent::FocusOut && object == text_) {
        CommitEdit();
    } else if (event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->modifiers() == Qt::NoModifier) {
            // In edit mode Space types a space; otherwise Space/Escape toggle
            // the preview closed.
            if (key->key() == Qt::Key_Escape) {
                emit dismissRequested();
                return true;
            }
            if (key->key() == Qt::Key_Space && !editing_) {
                emit dismissRequested();
                return true;
            }
            if ((key->key() == Qt::Key_Up || key->key() == Qt::Key_Down)
                && pages_->currentWidget() != text_) {
                emit navigateRequested(key->key() == Qt::Key_Up ? -1 : 1);
                return true;
            }
        }
    }
    return QWidget::eventFilter(object, event);
}
