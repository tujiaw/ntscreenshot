#include <QCoreApplication>
#include "LongScreenshotControlPanel.h"
#include "modules/capture/CaptureControlPanelStyle.h"
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

    const int btnSize = Util::scaleSize(CaptureControlPanelStyle::ButtonSize);
    const int iconSize = Util::scaleSize(CaptureControlPanelStyle::IconSize);

    infoLabel_ = new QLabel(this);
    infoLabel_->setObjectName(QStringLiteral("longScreenshotInfoLabel"));
    infoLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    infoLabel_->setText(QCoreApplication::translate("App", "请回滚重试"));
    infoLabel_->setToolTip(QCoreApplication::translate("App", "匹配中断，请放慢并回滚至已截取位置"));
    infoLabel_->setStyleSheet(QStringLiteral("color: %1; padding: 0 4px;")
                             .arg(ThemeManager::tokens().warning.name()));
    infoLabel_->hide();

    finishButton_ = new QPushButton(
        ThemeIcon::icon(QStringLiteral("ok.png"), IconTone::OnAccent, iconSize), QString(), this);
    finishButton_->setObjectName(QStringLiteral("longScreenshotFinishButton"));
    finishButton_->setToolTip(QCoreApplication::translate("App", "完成"));
    finishButton_->setFixedSize(btnSize, btnSize);
    finishButton_->setIconSize(QSize(iconSize, iconSize));
    UiStyler::setRole(finishButton_, UiRole::PrimaryButton);
    finishButton_->ensurePolished();
    finishButton_->setFixedSize(btnSize, btnSize);
    connect(finishButton_, &QPushButton::clicked, this, &LongScreenshotControlPanel::sigFinishClicked);

    cancelButton_ = new QPushButton(
        ThemeIcon::icon(QStringLiteral("remove.png"), IconTone::Danger, iconSize), QString(), this);
    cancelButton_->setObjectName(QStringLiteral("longScreenshotCancelButton"));
    cancelButton_->setToolTip(QCoreApplication::translate("App", "取消 (ESC)"));
    cancelButton_->setFixedSize(btnSize, btnSize);
    cancelButton_->setIconSize(QSize(iconSize, iconSize));
    UiStyler::setRole(cancelButton_, UiRole::DangerButton);
    cancelButton_->ensurePolished();
    cancelButton_->setFixedSize(btnSize, btnSize);
    connect(cancelButton_, &QPushButton::clicked, this, &LongScreenshotControlPanel::sigCancelClicked);

    QHBoxLayout* hLayout = new QHBoxLayout(this);
    CaptureControlPanelStyle::setup(this, hLayout);
    hLayout->addWidget(infoLabel_);
    hLayout->addWidget(finishButton_);
    hLayout->addWidget(cancelButton_);

    hLayout->setSizeConstraint(QLayout::SetFixedSize);
}

void LongScreenshotControlPanel::setRecoveryWarning(bool visible)
{
    if (visible == !infoLabel_->isHidden()) return;
    infoLabel_->setVisible(visible);
    layout()->activate();
    adjustSize();
}

void LongScreenshotControlPanel::paintEvent(QPaintEvent *)
{
    CaptureControlPanelStyle::paint(this);
}
