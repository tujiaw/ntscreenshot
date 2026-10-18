#include "ClipboardPreviewWindow.h"

#include <QApplication>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPlainTextEdit>
#include <QResizeEvent>
#include <QScreen>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <cmath>

namespace {
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

ClipboardPreviewWindow::ClipboardPreviewWindow(QWidget* owner)
    : QWidget(owner, Qt::Tool | Qt::WindowStaysOnTopHint), owner_(owner) {
    setObjectName(QStringLiteral("clipboardPreviewWindow"));
    setWindowTitle(QCoreApplication::translate("App", "预览"));
    setAttribute(Qt::WA_ShowWithoutActivating);
    setMinimumSize(240, 160);
    resize(560, 420);
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    pages_ = new QStackedWidget(this);
    root->addWidget(pages_);
    text_ = new QPlainTextEdit(pages_);
    text_->setObjectName(QStringLiteral("clipboardPreviewText"));
    text_->setReadOnly(true);
    text_->setUndoRedoEnabled(false);
    text_->setFrameShape(QFrame::NoFrame);
    text_->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    pages_->addWidget(text_);
    image_ = new PreviewImageView(pages_);
    image_->setObjectName(QStringLiteral("clipboardPreviewImage"));
    pages_->addWidget(image_);
    error_ = new QLabel(QCoreApplication::translate("App", "无法预览此图片"), pages_);
    error_->setAlignment(Qt::AlignCenter);
    pages_->addWidget(error_);
    qApp->installEventFilter(this);
}

void ClipboardPreviewWindow::ShowItem(const ClipItem& item) {
    if (!hasContent_ || current_.kind != item.kind || current_.text != item.text || current_.data != item.data) {
        current_ = item;
        hasContent_ = true;
        if (item.kind == ClipKind::Text) {
            static_cast<PreviewImageView*>(image_)->SetImage({});
            text_->setPlainText(item.text);
            pages_->setCurrentWidget(text_);
        } else {
            text_->clear();
            const QPixmap pixmap = QPixmap::fromImage(QImage::fromData(item.data));
            static_cast<PreviewImageView*>(image_)->SetImage(pixmap);
            pages_->setCurrentWidget(pixmap.isNull() ? static_cast<QWidget*>(error_) : image_);
        }
    }
    if (isVisible()) return;
    const QRect area = owner_->screen() ? owner_->screen()->availableGeometry() : owner_->frameGeometry();
    resize(qMin(width(), area.width()), qMin(height(), area.height()));
    const QRect anchor = owner_->frameGeometry();
    int x = anchor.left() - width() - 8;
    if (x < area.left()) x = anchor.right() + 9;
    x = qBound(area.left(), x, qMax(area.left(), area.right() - width() + 1));
    const int y = qBound(area.top(), anchor.top(), qMax(area.top(), area.bottom() - height() + 1));
    move(x, y);
    show();
    raise();
}

void ClipboardPreviewWindow::CloseIfFocusOutside() {
    if (!isVisible()) return;
    auto* active = QApplication::activeWindow();
    if (active == owner_ || active == this) return;
    if (qobject_cast<QMenu*>(active)) {
        for (auto* parent = active->parentWidget(); parent; parent = parent->parentWidget()) {
            if (parent == owner_ || parent == this) return;
        }
    }
    hide();
}

bool ClipboardPreviewWindow::eventFilter(QObject* object, QEvent* event) {
    if (!isVisible()) return QWidget::eventFilter(object, event);
    if (event->type() == QEvent::ApplicationDeactivate) {
        hide();
    } else if (event->type() == QEvent::WindowDeactivate && (object == owner_ || object == this)) {
        QTimer::singleShot(40, this, &ClipboardPreviewWindow::CloseIfFocusOutside);
    } else if (event->type() == QEvent::KeyPress) {
        auto* target = qobject_cast<QWidget*>(object);
        if (target && target->window() == this) {
            auto* key = static_cast<QKeyEvent*>(event);
            if (key->key() == Qt::Key_Escape || (key->key() == Qt::Key_Space && key->modifiers() == Qt::NoModifier)) {
                hide(); return true;
            }
            if (key->modifiers() == Qt::NoModifier && (key->key() == Qt::Key_Up || key->key() == Qt::Key_Down)
                && pages_->currentWidget() != text_) {
                emit navigateRequested(key->key() == Qt::Key_Up ? -1 : 1);
                return true;
            }
        }
    }
    return QWidget::eventFilter(object, event);
}
