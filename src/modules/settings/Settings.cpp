#include "Settings.h"
#include "SettingsInternal.h"

#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <QStyle>
#include <QTabBar>
#include <QTimer>
#include <QVBoxLayout>

#include "app/WindowManager.h"
#include "core/platform/Util.h"
#include "core/settings/SettingModel.h"
#include "core/theme/DarkStyle.h"
#include "core/theme/UiStyler.h"
#include "modules/local_search/infrastructure/SearchIndexStore.h"

Settings::Settings(WindowManager* windowManager, QWidget *parent)
    : QDialog(parent)
    , windowManager_(windowManager)
    , cbTheme_(nullptr)

{
	qInfo() << "Settings: constructor start";
	// Prevent background SQLite operations from interfering with widget
	// construction — QSqlDatabase::addDatabase is not re-entrant safe.
	SearchIndexStore::pauseBackgroundAccess();
	ui.setupUi(this);
	// Keep language names in their native scripts even if a translation catalog
	// contains an empty or unfinished entry for an already-English UI string.
	ui.cbLanguage->setItemText(0, QStringLiteral("English"));
	ui.cbLanguage->setItemText(1, QStringLiteral("中文"));
	ui.cbLanguage->setMinimumWidth(Util::scaleSize(124));
	ui.labelBuildTime->setText(QCoreApplication::translate("App", "编译时间：") + QStringLiteral(__DATE__) + QStringLiteral(" ") + QStringLiteral(__TIME__));
	SearchIndexStore::resumeBackgroundAccess();
	qInfo() << "Settings: UI setup done, initializing tabs...";

	// 快捷键状态按钮：统一尺寸，去除默认内边距
	int btnSize = Util::scaleSize(28);
	int iconSize = Util::scaleSize(18);
	auto setupStatusBtn = [btnSize, iconSize](QPushButton* btn, const QString& icon, const QString& tooltip) {
	    btn->setFixedSize(btnSize, btnSize);
	    btn->setIconSize(QSize(iconSize, iconSize));
	    btn->setIcon(QIcon(icon));
	    btn->setToolTip(tooltip);
	    btn->setStyleSheet(QStringLiteral("padding: 0px;"));
	};
	setupStatusBtn(ui.pbScreenshotStatus, ":/images/ok.png", QCoreApplication::translate("App", "快捷键注册状态"));
	setupStatusBtn(ui.pbPinStatus, ":/images/ok.png", QCoreApplication::translate("App", "快捷键注册状态"));

    // 适配路径相关按钮
    int pathBtnWidth = Util::scaleSize(75);
    int pathBtnHeight = Util::scaleSize(25);
    ui.pbOpenImagePath->setMinimumSize(pathBtnWidth, pathBtnHeight);
    ui.pbOpenImagePath->setMaximumSize(pathBtnWidth, pathBtnHeight);
    ui.pbModifyImagePath->setMinimumSize(pathBtnWidth, pathBtnHeight);
    ui.pbModifyImagePath->setMaximumSize(pathBtnWidth, pathBtnHeight);

    QPixmap tipsPixmap(QString(":/images/tips.png"));
    ui.labelTips->setPixmap(tipsPixmap);
    ui.labelTips->setToolTip(QCoreApplication::translate("App", "在输入框上按下要设置的快捷键"));
    qInfo() << "Settings: initializing tabs...";
    initTablePath();
    qInfo() << "Settings: initTablePath done";
    initThemeSelector();
    initCursorSelector();
    initSelectionBorderColor();
    initAmplifierCrossColor();
    arrangeGeneralSettings();
    qInfo() << "Settings: initThemeSelector done";

    initLocalSearchTab();
    qInfo() << "Settings: initLocalSearchTab done";
    initHttpServerTab();
    qInfo() << "Settings: initHttpServerTab done";
    applyModernLayout();
    qInfo() << "Settings: modern layout applied";

    // 使用系统原生标题栏（与 AI Fill Settings 一致），仅设置标题与关闭清理
    setAttribute(Qt::WA_DeleteOnClose, true);
    setWindowTitle(QCoreApplication::translate("App", "设置"));

    connect(ui.cbAutoStart, &QCheckBox::clicked, this, &Settings::onAutoStartClicked);
    connect(ui.cbLanguage, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Settings::onLanguageChanged);
    connect(ui.cbAutoPin, &QCheckBox::clicked, this, &Settings::onAutoPin);
    connect(ui.cbPinNoBorder, &QCheckBox::clicked, this, &Settings::onPinNoBorder);
    connect(ui.pbRevert, &QPushButton::clicked, this, &Settings::onRevertClicked);
    connect(ui.rbRGB, &QRadioButton::toggled, this, &Settings::onRgbColorToggled);
    connect(ui.rbHexadecimal, &QRadioButton::toggled, this, &Settings::onHexColorToggled);
    connect(ui.cbAutoSave, &QCheckBox::clicked, this, &Settings::onAutoSaveChanged);
    connect(ui.pbOpenImagePath, &QPushButton::clicked, this, &Settings::onOpenImagePath);
    connect(ui.pbModifyImagePath, &QPushButton::clicked, this, &Settings::onModifyImagePath);
    connect(ui.cbBackground, &QCheckBox::clicked, this, &Settings::onBackgroundChanged);
    connect(ui.hsBackgroundAlpha, &QSlider::sliderReleased, this, &Settings::onBackgroundAlphaReleased);
    connect(ui.leGitHubOwner,      &QLineEdit::textChanged, this, &Settings::onGitHubFieldChanged);
    connect(ui.leGitHubRepo,       &QLineEdit::textChanged, this, &Settings::onGitHubFieldChanged);
    connect(ui.leGitHubBranch,     &QLineEdit::textChanged, this, &Settings::onGitHubFieldChanged);
    connect(ui.leGitHubToken,      &QLineEdit::textChanged, this, &Settings::onGitHubFieldChanged);
    connect(ui.leGitHubPathPrefix, &QLineEdit::textChanged, this, &Settings::onGitHubFieldChanged);
    connect(ui.leGitHubCdnUrl,     &QLineEdit::textChanged, this, &Settings::onGitHubFieldChanged);
    connect(ui.cbPaddleOcrEnabled, &QCheckBox::toggled, this, &Settings::onPaddleOcrChanged);
    connect(ui.lePaddleOcrUrl, &QLineEdit::textChanged, this, &Settings::onPaddleOcrChanged);
    connect(ui.lePaddleOcrToken, &QLineEdit::textChanged, this, &Settings::onPaddleOcrChanged);
    connect(ui.lePaddleOcrModel, &QLineEdit::textChanged, this, &Settings::onPaddleOcrChanged);

    connect(ui.kseScreenshot, &QKeySequenceEdit::editingFinished, this, &Settings::updateScreenshotGlobalKey);
    connect(ui.ksePin, &QKeySequenceEdit::editingFinished, this, &Settings::updatePinKey);

    qInfo() << "Settings: reading data...";
    readData();
    qInfo() << "Settings: readData done";

	// 使用通用DPI适配方法调整窗口大小和字体
	Util::scaleWidgetDPI(this, 840, 560, true);

    statusTipTimer_ = new QTimer(this);
    statusTipTimer_->setSingleShot(true);
    connect(statusTipTimer_, &QTimer::timeout, this, [this]() {
        ui.labelStatusTip->clear();
    });

	qInfo() << "Settings: constructor done";
}

void Settings::showStatusTip(const QString& msg, bool ok)
{
    if (!ui.labelStatusTip) return;
    ui.labelStatusTip->setText(msg);
    ui.labelStatusTip->setProperty("status", ok ? QStringLiteral("success")
                                                  : QStringLiteral("error"));
    ui.labelStatusTip->style()->unpolish(ui.labelStatusTip);
    ui.labelStatusTip->style()->polish(ui.labelStatusTip);
    statusTipTimer_->start(4000);  // auto-clear after 4s
}

Settings::~Settings()
{
}

void Settings::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && !event->modifiers()) {
        // 对话框语义：Esc 关闭设置窗口
        close();
        return;
    }
    QDialog::keyPressEvent(event);
}

void Settings::arrangeGeneralSettings()
{
    auto widgetsInRow = [this](const char* name) {
        QList<QWidget*> widgets;
        auto* layout = findChild<QHBoxLayout*>(QLatin1String(name));
        if (layout) {
            for (int i = 0; i < layout->count(); ++i) {
                if (auto* widget = layout->itemAt(i)->widget()) widgets.append(widget);
            }
        }
        return widgets;
    };
    const auto theme = widgetsInRow("generalThemeRow");
    const auto cursor = widgetsInRow("generalCursorRow");
    const auto borderColor = widgetsInRow("generalBorderColorRow");
    const auto borderStyle = widgetsInRow("generalBorderStyleRow");
    const auto crossColor = widgetsInRow("generalCrossColorRow");

    // Reuse the controls and their connections, replacing only the old row layouts.
    std::function<void(QLayout*)> clearLayout = [&](QLayout* layout) {
        while (auto* item = layout->takeAt(0)) {
            if (auto* child = item->layout()) clearLayout(child);
            delete item;
        }
    };
    clearLayout(ui.verticalLayout_2);
    ui.horizontalLayout_8->removeWidget(ui.label_8);
    ui.horizontalLayout_8->setContentsMargins(0, 0, 0, 0);
    ui.horizontalLayout_8->setSpacing(Util::scaleSize(12));
    for (auto* radio : {ui.rbRGB, ui.rbHexadecimal}) {
        radio->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    }
    ui.label_8->setText(QCoreApplication::translate("App", "颜色格式"));
    qobject_cast<QLabel*>(theme[0])->setText(QCoreApplication::translate("App", "主题"));
    for (auto* label : {qobject_cast<QLabel*>(theme[0]), qobject_cast<QLabel*>(cursor[0]),
                        qobject_cast<QLabel*>(borderColor[0]), qobject_cast<QLabel*>(borderStyle[0]),
                        qobject_cast<QLabel*>(crossColor[0]), ui.label_8, ui.labelLanguage,
                        ui.label, ui.label_2}) {
        QString text = label->text().trimmed();
        while (text.endsWith(QLatin1Char(':')) || text.endsWith(QChar(0xff1a))) text.chop(1);
        label->setText(text + QCoreApplication::translate("App", "："));
    }
    ui.widget->setToolTip(QCoreApplication::translate("App", "颜色格式显示(C键复制颜色)："));
    cursor[3]->setStyleSheet(QStringLiteral("color: palette(mid);"));

    auto* grid = new QGridLayout();
    grid->setObjectName(QStringLiteral("generalSettingsGrid"));
    grid->setHorizontalSpacing(Util::scaleSize(12));
    grid->setVerticalSpacing(Util::scaleSize(6));
    // Keep the form compact; spare width belongs after the action column.
    grid->setColumnStretch(3, 1);
    const int controlWidth = Util::scaleSize(160);
    for (QWidget* control : {static_cast<QWidget*>(cbCursor_),
                            static_cast<QWidget*>(selectionBorderColorButton_),
                            static_cast<QWidget*>(selectionBorderStyleCombo_),
                            static_cast<QWidget*>(amplifierCrossColorButton_)}) {
        control->setMinimumWidth(controlWidth);
        control->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }
    auto addRow = [grid](int row, const QList<QWidget*>& widgets) {
        // Keep labels, controls and reset buttons in consistent columns.
        for (int column = 0; column < 3; ++column) {
            grid->addWidget(widgets[column], row, column,
                            column == 2 ? Qt::AlignLeft : Qt::Alignment());
        }
    };
    auto* appearance = new QHBoxLayout();
    appearance->setSpacing(Util::scaleSize(12));
    appearance->addWidget(ui.cbAutoStart);
    appearance->addSpacing(Util::scaleSize(12));
    appearance->addWidget(theme[0]);
    appearance->addWidget(cbTheme_);
    appearance->addSpacing(Util::scaleSize(12));
    appearance->addWidget(ui.labelLanguage);
    appearance->addWidget(ui.cbLanguage);
    appearance->addStretch();
    for (auto* combo : {cbTheme_, ui.cbLanguage}) {
        combo->setMinimumWidth(0);
        combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        combo->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    }
    grid->addLayout(appearance, 0, 0, 1, 4);
    grid->addWidget(cursor[0], 1, 0);
    grid->addWidget(cursor[1], 1, 1);
    auto* cursorHelp = new QHBoxLayout();
    cursorHelp->setSpacing(Util::scaleSize(8));
    cursorHelp->addWidget(cursor[2]);
    cursorHelp->addWidget(cursor[3], 1);
    grid->addLayout(cursorHelp, 1, 2, 1, 2);
    addRow(2, borderColor);
    addRow(3, borderStyle);
    addRow(4, crossColor);
    grid->addWidget(ui.label_8, 5, 0);
    grid->addWidget(ui.widget, 5, 1, 1, 3);
    for (auto* label : {qobject_cast<QLabel*>(cursor[0]), qobject_cast<QLabel*>(borderColor[0]),
                        qobject_cast<QLabel*>(borderStyle[0]), qobject_cast<QLabel*>(crossColor[0]),
                        ui.label_8, ui.label, ui.label_2}) {
        label->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    }
    ui.kseScreenshot->setMinimumWidth(Util::scaleSize(100));
    ui.ksePin->setMinimumWidth(Util::scaleSize(100));
    grid->addWidget(ui.label, 6, 0);
    grid->addWidget(ui.kseScreenshot, 6, 1);
    grid->addWidget(ui.pbScreenshotStatus, 6, 2, Qt::AlignLeft);
    grid->addWidget(ui.label_2, 7, 0);
    grid->addWidget(ui.ksePin, 7, 1);
    grid->addWidget(ui.pbPinStatus, 7, 2, Qt::AlignLeft);
    ui.labelTips->hide();
    ui.kseScreenshot->setToolTip(ui.labelTips->toolTip());
    ui.ksePin->setToolTip(ui.labelTips->toolTip());
    ui.cbBackground->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    auto* backgroundOptions = new QHBoxLayout();
    backgroundOptions->setSpacing(Util::scaleSize(12));
    backgroundOptions->addWidget(ui.cbBackground);
    backgroundOptions->addWidget(ui.hsBackgroundAlpha);
    backgroundOptions->addStretch();
    grid->addLayout(backgroundOptions, 8, 0, 1, 4);
    auto* pinOptions = new QHBoxLayout();
    pinOptions->setSpacing(Util::scaleSize(20));
    pinOptions->addWidget(ui.cbAutoPin);
    pinOptions->addWidget(ui.cbPinNoBorder);
    pinOptions->addStretch();
    grid->addLayout(pinOptions, 9, 0, 1, 4);
    ui.verticalLayout_2->addLayout(grid);
    ui.verticalLayout_2->addStretch();
}

void Settings::applyModernLayout()
{
    QTabWidget* legacyTabs = ui.tabWidget;
    if (!legacyTabs || !ui.verticalLayout) return;

    auto pageByTitle = [legacyTabs](const QString& title) -> QWidget* {
        for (int i = 0; i < legacyTabs->count(); ++i) {
            if (legacyTabs->tabText(i) == title) return legacyTabs->widget(i);
        }
        return nullptr;
    };

    QWidget* generalPage = pageByTitle(QCoreApplication::translate("App", "常规设置"));
    QWidget* imagePage = pageByTitle(QCoreApplication::translate("App", "图片"));
    QWidget* pathPage = pageByTitle(QCoreApplication::translate("App", "路径"));

    QWidget* githubPage = pageByTitle(QCoreApplication::translate("App", "GitHub图床"));
    QWidget* localSearchPage = pageByTitle(QCoreApplication::translate("App", "本地搜索"));
    QWidget* httpServerPage = pageByTitle(QCoreApplication::translate("App", "HTTP 服务"));
    QWidget* aboutPage = pageByTitle(QCoreApplication::translate("App", "关于"));

    // Keep Designer-owned pages inside their original QTabWidgets. Reparenting
    // them at runtime leaves Qt with stale explicit show/hide state and can
    // crash while the window is polished. Navigation below only switches the
    // existing stacks, so object names, layouts and signal wiring stay intact.
    ui.verticalLayout->removeWidget(legacyTabs);
    legacyTabs->setDocumentMode(true);
    legacyTabs->tabBar()->hide();

    auto* shell = new QFrame(this);
    shell->setObjectName(QStringLiteral("settingsShell"));
    auto* shellLayout = new QHBoxLayout(shell);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);

    auto* navigation = new QListWidget(shell);
    navigation->setObjectName(QStringLiteral("settingsNavigation"));
    navigation->setFixedWidth(Util::scaleSize(168));
    navigation->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    navigation->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    navigation->setFocusPolicy(Qt::NoFocus);
    navigation->addItems({QCoreApplication::translate("App", "常规"), QCoreApplication::translate("App", "截图与输出"),
                          QCoreApplication::translate("App", "本地搜索"), QCoreApplication::translate("App", "HTTP 服务"),
                          QCoreApplication::translate("App", "路径记录"),
                          QCoreApplication::translate("App", "关于")});

    auto* content = new QWidget(shell);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(Util::scaleSize(20), Util::scaleSize(18),
                                      Util::scaleSize(20), Util::scaleSize(18));
    contentLayout->setSpacing(Util::scaleSize(6));
    auto* titleLabel = new QLabel(content);
    titleLabel->setObjectName(QStringLiteral("settingsPageTitle"));
    QFont titleFont = titleLabel->font();
    titleFont.setPixelSize(Util::scaleSize(18));
    titleFont.setWeight(QFont::DemiBold);
    titleLabel->setFont(titleFont);
    auto* subtitleLabel = new QLabel(content);
    subtitleLabel->setObjectName(QStringLiteral("settingsPageSubtitle"));
    auto* subNavigation = new QWidget(content);
    subNavigation->setObjectName(QStringLiteral("settingsSubNavigation"));
    auto* subLayout = new QHBoxLayout(subNavigation);
    subLayout->setContentsMargins(0, Util::scaleSize(4), 0, Util::scaleSize(4));
    subLayout->setSpacing(Util::scaleSize(6));
    contentLayout->addWidget(titleLabel);
    contentLayout->addWidget(subtitleLabel);
    // 标题区与内容之间的分隔线
    auto* titleDivider = new QFrame(content);
    titleDivider->setObjectName(QStringLiteral("settingsTitleDivider"));
    titleDivider->setFixedHeight(Util::scaleSize(1));
    contentLayout->addWidget(titleDivider);
    contentLayout->addWidget(subNavigation);
    contentLayout->addWidget(legacyTabs, 1);

    auto setSubPages = [subNavigation, subLayout](
                           const QList<QPair<QString, std::function<void()>>>& pages) {
        while (QLayoutItem* item = subLayout->takeAt(0)) {
            if (QWidget* widget = item->widget()) widget->deleteLater();
            delete item;
        }
        for (const auto& page : pages) {
            auto* button = new QPushButton(page.first, subNavigation);
            button->setProperty("uiRole", QStringLiteral("secondary"));
            button->setFixedHeight(Util::scaleSize(30));
            QObject::connect(button, &QPushButton::clicked, subNavigation, page.second);
            subLayout->addWidget(button);
        }
        subLayout->addStretch();
        subNavigation->setVisible(!pages.isEmpty());
    };

    auto selectPage = [=](int row) {
        QList<QPair<QString, std::function<void()>>> subPages;
        switch (row) {
        case 0:
            titleLabel->setText(QCoreApplication::translate("App", "常规"));
            subtitleLabel->setText(QCoreApplication::translate("App", "管理外观、启动和快捷键"));
            legacyTabs->setCurrentWidget(generalPage);
            break;
        case 1:
            titleLabel->setText(QCoreApplication::translate("App", "截图与输出"));
            subtitleLabel->setText(QCoreApplication::translate("App", "管理图片保存、OCR 与图床"));
            legacyTabs->setCurrentWidget(imagePage);
            subPages = {
                {QCoreApplication::translate("App", "图片与 OCR"), [=] { legacyTabs->setCurrentWidget(imagePage); }},
                {QCoreApplication::translate("App", "GitHub 图床"), [=] { legacyTabs->setCurrentWidget(githubPage); }}
            };
            break;
        case 2:
            titleLabel->setText(QCoreApplication::translate("App", "本地搜索"));
            subtitleLabel->setText(QCoreApplication::translate("App", "管理索引范围、排除规则和搜索引擎"));
            legacyTabs->setCurrentWidget(localSearchPage);
            break;
        case 3:
            titleLabel->setText(QCoreApplication::translate("App", "HTTP 服务"));
            subtitleLabel->setText(QCoreApplication::translate("App", "共享本地目录并管理后台服务"));
            legacyTabs->setCurrentWidget(httpServerPage);
            break;
        case 4:
            titleLabel->setText(QCoreApplication::translate("App", "路径记录"));
            subtitleLabel->setText(QCoreApplication::translate("App", "查看并复制常用保存路径"));
            legacyTabs->setCurrentWidget(pathPage);
            break;
        default:
            titleLabel->setText(QCoreApplication::translate("App", "关于"));
            subtitleLabel->setText(QCoreApplication::translate("App", "版本信息与项目说明"));
            legacyTabs->setCurrentWidget(aboutPage);
            break;
        }
        setSubPages(subPages);
    };

    shellLayout->addWidget(navigation);
    shellLayout->addWidget(content, 1);
    ui.verticalLayout->insertWidget(0, shell, 1);
    connect(navigation, &QListWidget::currentRowChanged, this, selectPage);
    navigation->setCurrentRow(0);

    UiStyler::setRole(ui.pbRevert, UiRole::SecondaryButton);
    // Keep the existing height, but let longer translations use their natural width.
    ui.pbRevert->setFixedHeight(Util::scaleSize(34));
    ui.pbRevert->setMinimumWidth(qMax(Util::scaleSize(88),
        ui.pbRevert->fontMetrics().horizontalAdvance(ui.pbRevert->text()) + Util::scaleSize(36)));
    ui.labelStatusTip->setObjectName(QStringLiteral("settingsStatusTip"));
    setWindowTitle(QCoreApplication::translate("App", "ntscreenshot 设置"));
}

void Settings::readData()
{
    qInfo() << "Settings::readData: start";
    SettingModel *setting = windowManager_->setting();
    {
        const QSignalBlocker blocker(ui.cbLanguage);
        ui.cbLanguage->setCurrentIndex(setting->uiLanguage() == QStringLiteral("zh") ? 1 : 0);
    }
    ui.cbAutoStart->setCheckState(setting->autoStart() ? Qt::Checked : Qt::Unchecked);
    ui.cbAutoPin->setCheckState(setting->autoPin() ? Qt::Checked : Qt::Unchecked);
    ui.cbPinNoBorder->setCheckState(setting->pinNoBorder() ? Qt::Checked : Qt::Unchecked);
    ui.kseScreenshot->setKeySequence(QKeySequence::fromString(setting->screenhotGlobalKey(), QKeySequence::NativeText));
    ui.ksePin->setKeySequence(QKeySequence::fromString(setting->pinGlobalKey(), QKeySequence::NativeText));

    settings_detail::setStatusIcon(ui.pbScreenshotStatus, !setting->screenhotGlobalKey().isEmpty());
    settings_detail::setStatusIcon(ui.pbPinStatus, !setting->pinGlobalKey().isEmpty());

    ui.rbRGB->setChecked(setting->rgbColor());
    ui.rbHexadecimal->setChecked(!setting->rgbColor());
    {
        const QSignalBlocker cursorBlocker(cbCursor_);
        cbCursor_->setCurrentIndex(cbCursor_->findData(static_cast<int>(setting->screenshotCursorStyle())));
    }
    refreshCursorPreview();
    refreshSelectionBorderColor();
    refreshAmplifierCrossColor();

    bool autoSave = false;
    QString autoSavePath;
    setting->getAutoSaveImage(autoSave, autoSavePath);
    ui.cbAutoSave->setChecked(autoSave);
    ui.leSavePath->setText(autoSavePath);
    ui.pbOpenImagePath->setEnabled(true);
    ui.pbModifyImagePath->setEnabled(autoSave);

    const PaddleOcrConfig ocrConfig = setting->paddleOcrConfig();
    {
        const QSignalBlocker enabledBlocker(ui.cbPaddleOcrEnabled);
        const QSignalBlocker urlBlocker(ui.lePaddleOcrUrl);
        const QSignalBlocker tokenBlocker(ui.lePaddleOcrToken);
        const QSignalBlocker modelBlocker(ui.lePaddleOcrModel);
        ui.cbPaddleOcrEnabled->setChecked(ocrConfig.enabled);
        ui.lePaddleOcrUrl->setText(ocrConfig.jobUrl);
        ui.lePaddleOcrToken->setText(ocrConfig.token);
        ui.lePaddleOcrModel->setText(ocrConfig.model);
    }
    ui.lePaddleOcrUrl->setEnabled(ocrConfig.enabled);
    ui.lePaddleOcrToken->setEnabled(ocrConfig.enabled);
    ui.lePaddleOcrModel->setEnabled(ocrConfig.enabled);

    ui.cbBackground->setChecked(setting->backgroundColorChecked());
    ui.hsBackgroundAlpha->setValue(setting->backgroundColorAlpha());
    ui.hsBackgroundAlpha->setEnabled(ui.cbBackground->isChecked());
    if (cbTheme_) {
        const QSignalBlocker blocker(cbTheme_);
        const int index = cbTheme_->findData(static_cast<int>(setting->themeMode()));
        cbTheme_->setCurrentIndex(index >= 0 ? index : 0);
    }

    const GitHubImageBedConfig ghConfig = setting->gitHubImageBedConfig();
    ui.leGitHubOwner->setText(ghConfig.owner);
    ui.leGitHubRepo->setText(ghConfig.repo);
    ui.leGitHubBranch->setText(ghConfig.branch);
    ui.leGitHubToken->setText(ghConfig.token);
    ui.leGitHubPathPrefix->setText(ghConfig.pathPrefix);
    ui.leGitHubCdnUrl->setText(ghConfig.cdnUrl);
    loadLocalSearchSettings();
    qInfo() << "Settings::readData: done";
}
