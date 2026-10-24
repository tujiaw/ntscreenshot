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
    // The capture hint is intentionally omitted. A changing label resizes the
    // native floating panel and makes it jump while frames are captured.
    infoLabel_->setVisible(false);

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
    CaptureControlPanelStyle::paint(this);
}
