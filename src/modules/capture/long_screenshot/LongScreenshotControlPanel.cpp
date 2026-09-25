#include "LongScreenshotControlPanel.h"
#include <QHBoxLayout>
#include <QPainter>
#include "core/platform/Util.h"
#include "core/theme/ThemeIcon.h"
#include "core/theme/ThemeManager.h"
#include "core/theme/UiStyler.h"

LongScreenshotControlPanel::LongScreenshotControlPanel(QWidget *parent)
    : QWidget(parent)
    , infoLabel_(nullptr)
    , cancelButton_(nullptr)
    , finishButton_(nullptr)
{
    setupUI();
}

LongScreenshotControlPanel::~LongScreenshotControlPanel()
{
}

void LongScreenshotControlPanel::setupUI()
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setObjectName(QStringLiteral("LongScreenshotControlPanel"));

    const int btnSize = Util::scaleSize(30);
    const int iconSize = Util::scaleSize(16);

    infoLabel_ = new QLabel(this);
    infoLabel_->setObjectName(QStringLiteral("longScreenshotInfoLabel"));
    infoLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    // The capture hint is intentionally omitted. A changing label resizes the
    // native floating panel and makes it jump while frames are captured.
    infoLabel_->setVisible(false);

    finishButton_ = new QPushButton(
        ThemeIcon::icon(QStringLiteral("ok.png"), IconTone::OnAccent, iconSize), QString(), this);
    finishButton_->setObjectName(QStringLiteral("longScreenshotFinishButton"));
    finishButton_->setToolTip(QStringLiteral("完成"));
    finishButton_->setFixedSize(btnSize, btnSize);
    finishButton_->setIconSize(QSize(iconSize, iconSize));
    UiStyler::setRole(finishButton_, UiRole::PrimaryButton);
    connect(finishButton_, &QPushButton::clicked, this, &LongScreenshotControlPanel::sigFinishClicked);

    cancelButton_ = new QPushButton(
        ThemeIcon::icon(QStringLiteral("remove.png"), IconTone::Danger, iconSize), QString(), this);
    cancelButton_->setObjectName(QStringLiteral("longScreenshotCancelButton"));
    cancelButton_->setToolTip(QStringLiteral("取消 (ESC)"));
    cancelButton_->setFixedSize(btnSize, btnSize);
    cancelButton_->setIconSize(QSize(iconSize, iconSize));
    UiStyler::setRole(cancelButton_, UiRole::DangerButton);
    connect(cancelButton_, &QPushButton::clicked, this, &LongScreenshotControlPanel::sigCancelClicked);

    QHBoxLayout* hLayout = new QHBoxLayout(this);
    Util::scaleLayoutMargins(hLayout, 10, 6, 6, 6);
    hLayout->setSpacing(Util::scaleSize(4));
    hLayout->addWidget(finishButton_);
    hLayout->addWidget(cancelButton_);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void LongScreenshotControlPanel::setInfoText(const QString &text)
{
    if (infoLabel_) {
        infoLabel_->setText(text);
        infoLabel_->adjustSize();
        adjustSize();
    }
}

void LongScreenshotControlPanel::paintEvent(QPaintEvent *)
{
    // 透明窗口上手动绘制不透明的圆角面板背景
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const ThemeTokens& tokens = ThemeManager::tokens();
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(QPen(tokens.border, 1));
    painter.setBrush(tokens.surfaceRaised);
    painter.drawRoundedRect(r, Util::scaleSize(8), Util::scaleSize(8));
}
