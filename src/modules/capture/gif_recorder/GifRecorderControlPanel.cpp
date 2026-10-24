#include <QCoreApplication>
#include "GifRecorderControlPanel.h"
#include "modules/capture/CaptureControlPanelStyle.h"

#include "core/theme/ThemeIcon.h"
#include "core/theme/ThemeManager.h"
#include "core/platform/Util.h"

#include <QPainter>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QStackedWidget>

GifRecorderControlPanel::GifRecorderControlPanel(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setObjectName(QStringLiteral("GifRecorderControlPanel"));

    auto *layout = new QHBoxLayout(this);
    CaptureControlPanelStyle::setup(this, layout);
    const int buttonSize = Util::scaleSize(CaptureControlPanelStyle::ButtonSize);

    auto *fpsLabel = new QLabel(QStringLiteral("FPS"), this);
    fpsLabel->setObjectName(QStringLiteral("gifFpsLabel"));
    fpsEdit_ = new QLineEdit(QStringLiteral("10"), this);
    fpsEdit_->setObjectName(QStringLiteral("gifFpsEdit"));
    fpsEdit_->setValidator(new QIntValidator(1, 30, fpsEdit_));
    fpsEdit_->setAlignment(Qt::AlignCenter);
    fpsEdit_->setMaxLength(2);
    fpsEdit_->ensurePolished();
    fpsEdit_->setFixedSize(Util::scaleSize(38), buttonSize);
    fpsEdit_->setToolTip(QCoreApplication::translate("App", "输入 FPS（1–30）"));

    sizeLabel_ = new QLabel(this);
    sizeLabel_->setObjectName(QStringLiteral("gifSizeLabel"));
    elapsedLabel_ = new QLabel(QStringLiteral("00:00 · 0"), this);
    elapsedLabel_->setObjectName(QStringLiteral("gifElapsedLabel"));
    sizeLabel_->ensurePolished();
    elapsedLabel_->ensurePolished();
    sizeLabel_->setMinimumWidth(sizeLabel_->fontMetrics().horizontalAdvance(QStringLiteral("9999×9999")));
    elapsedLabel_->setMinimumWidth(elapsedLabel_->fontMetrics().horizontalAdvance(QStringLiteral("000:00 · 99999")));
    recordButton_ = new QPushButton(QCoreApplication::translate("App", "录制"), this);
    copyButton_ = new QPushButton(QCoreApplication::translate("App", "完成/复制"), this);
    openButton_ = new QPushButton(QCoreApplication::translate("App", "完成/打开"), this);
    cancelButton_ = new QToolButton(this);

    recordButton_->setObjectName(QStringLiteral("gifRecordButton"));
    copyButton_->setObjectName(QStringLiteral("gifCopyButton"));
    copyButton_->setToolTip(QCoreApplication::translate("App", "复制 GIF 文件到剪贴板"));
    openButton_->setObjectName(QStringLiteral("gifOpenButton"));
    openButton_->setToolTip(QCoreApplication::translate("App", "打开已保存的 GIF 文件"));
    cancelButton_->setObjectName(QStringLiteral("gifCloseButton"));
    recordButton_->ensurePolished();
    copyButton_->ensurePolished();
    openButton_->ensurePolished();
    cancelButton_->ensurePolished();
    recordButton_->setFixedSize(Util::scaleSize(64), buttonSize);
    copyButton_->setMinimumWidth(copyButton_->fontMetrics().horizontalAdvance(copyButton_->text()) + Util::scaleSize(24));
    openButton_->setMinimumWidth(openButton_->fontMetrics().horizontalAdvance(openButton_->text()) + Util::scaleSize(24));
    copyButton_->setFixedHeight(buttonSize);
    openButton_->setFixedHeight(buttonSize);
    cancelButton_->setFixedSize(buttonSize, buttonSize);
    const int iconSize = Util::scaleSize(CaptureControlPanelStyle::IconSize);
    cancelButton_->setIconSize(QSize(iconSize, iconSize));
    cancelButton_->setIcon(ThemeIcon::icon(QStringLiteral("remove.png"), IconTone::Danger, iconSize));
    cancelButton_->setToolTip(QCoreApplication::translate("App", "取消录制 (Esc)"));

    layout->addWidget(fpsLabel);
    layout->addWidget(fpsEdit_);
    layout->addWidget(sizeLabel_);
    layout->addWidget(elapsedLabel_);
    // Reserve the same action area for preparing and recording so the panel
    // stays anchored when the finish buttons replace the record button.
    actions_ = new QStackedWidget(this);
    auto* preparing = new QWidget(actions_);
    auto* preparingLayout = new QHBoxLayout(preparing);
    preparingLayout->setContentsMargins(0, 0, 0, 0);
    preparingLayout->addStretch();
    preparingLayout->addWidget(recordButton_);
    auto* recording = new QWidget(actions_);
    auto* recordingLayout = new QHBoxLayout(recording);
    recordingLayout->setContentsMargins(0, 0, 0, 0);
    recordingLayout->setSpacing(Util::scaleSize(6));
    recordingLayout->addWidget(copyButton_);
    recordingLayout->addWidget(openButton_);
    actions_->addWidget(preparing);
    actions_->addWidget(recording);
    layout->addWidget(actions_);
    layout->addWidget(cancelButton_);

    connect(recordButton_, &QPushButton::clicked, this, &GifRecorderControlPanel::sigRecordClicked);
    connect(copyButton_, &QPushButton::clicked, this, &GifRecorderControlPanel::sigCopyClicked);
    connect(openButton_, &QPushButton::clicked, this, &GifRecorderControlPanel::sigOpenClicked);
    connect(cancelButton_, &QToolButton::clicked, this, &GifRecorderControlPanel::sigCancelClicked);
    setPreparing();
}

void GifRecorderControlPanel::paintEvent(QPaintEvent*)
{
    CaptureControlPanelStyle::paint(this);
}

void GifRecorderControlPanel::applyTheme()
{
}

int GifRecorderControlPanel::framesPerSecond() const
{
    bool ok = false;
    const int fps = fpsEdit_->text().toInt(&ok);
    return ok ? qBound(1, fps, 30) : 10;
}

void GifRecorderControlPanel::setCaptureSize(const QSize &size)
{
    sizeLabel_->setText(QStringLiteral("%1×%2").arg(size.width()).arg(size.height()));
}

void GifRecorderControlPanel::setElapsedMilliseconds(qint64 milliseconds, int frames)
{
    const qint64 totalSeconds = milliseconds / 1000;
    const qint64 minutes = totalSeconds / 60;
    const qint64 seconds = totalSeconds % 60;
    elapsedLabel_->setText(QStringLiteral("%1:%2 · %3")
                               .arg(minutes, 2, 10, QLatin1Char('0'))
                               .arg(seconds, 2, 10, QLatin1Char('0'))
                               .arg(frames));
}

void GifRecorderControlPanel::setPreparing()
{
    fpsEdit_->setEnabled(true);
    fpsEdit_->show();
    recordButton_->setEnabled(true);
    actions_->setCurrentIndex(0);
    recordButton_->setText(QCoreApplication::translate("App", "录制"));
    cancelButton_->setEnabled(true);
    cancelButton_->setToolTip(QCoreApplication::translate("App", "取消录制 (Esc)"));
    setElapsedMilliseconds(0, 0);
    adjustSize();
}

void GifRecorderControlPanel::setRecording()
{
    fpsEdit_->setEnabled(false);
    actions_->setCurrentIndex(1);
    copyButton_->setEnabled(true);
    openButton_->setEnabled(true);
    cancelButton_->setEnabled(true);
    setElapsedMilliseconds(0, 0);
    adjustSize();
}

void GifRecorderControlPanel::setStarting()
{
    fpsEdit_->setEnabled(false);
    recordButton_->setEnabled(false);
    copyButton_->setEnabled(false);
    openButton_->setEnabled(false);
    cancelButton_->setEnabled(true);
    elapsedLabel_->setText(QCoreApplication::translate("App", "准备中…"));
}

void GifRecorderControlPanel::setEncoding()
{
    fpsEdit_->setEnabled(false);
    recordButton_->setEnabled(false);
    copyButton_->setEnabled(false);
    openButton_->setEnabled(false);
    cancelButton_->setEnabled(false);
    elapsedLabel_->setText(QCoreApplication::translate("App", "保存中…"));
}
