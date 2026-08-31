#include "Settings.h"
#include <QDebug>
#include <QClipboard>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QKeySequenceEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTabBar>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QIcon>
#include <QSizePolicy>
#include <QStyle>
#include <QUuid>
#include <QMessageBox>
#include <QTimer>
#include <QKeyEvent>
#include <functional>
#include "core/theme/DarkStyle.h"
#include "core/theme/ThemeManager.h"
#include "core/theme/UiStyler.h"
#include "modules/assistant/runtime/tools/LlmTool.h"
#include "core/settings/SettingModel.h"
#include "core/platform/Util.h"
#include "app/WindowManager.h"
#include "shared/foundation/IgnorePatternMatcher.h"
#include "modules/local_search/infrastructure/SearchIndexStore.h"

namespace {
QString keySequenceText(const QKeySequence &sequence)
{
    return sequence.toString(QKeySequence::NativeText).trimmed();
}

void setStatusIcon(QPushButton *button, bool ok)
{
    button->setIcon(QIcon(ok ? ":/images/ok.png" : ":/images/remove.png"));
}
}

Settings::Settings(WindowManager* windowManager, QWidget *parent)
    : QDialog(parent)
    , windowManager_(windowManager)
    , cbTheme_(nullptr)
    , cbLlmProviders_(nullptr)
    , cbNetworkSearchProviders_(nullptr)
    , leLlmProviderName_(nullptr)
    , cbWebSearchEnabled_(nullptr)
    , cbTextSelectionEnabled_(nullptr)
    , leNetworkSearchApiKey_(nullptr)
    , cbImageTokenSaving_(nullptr)
    , listTextSelectionActions_(nullptr)
    , listToolItems_(nullptr)
    , cbToolAutoPermission_(nullptr)
    , leTextSelectionActionLabel_(nullptr)
    , teTextSelectionActionPrompt_(nullptr)
    , btnDelTextSelectionAction_(nullptr)
{
	qInfo() << "Settings: constructor start";
	// Prevent background SQLite operations from interfering with widget
	// construction — QSqlDatabase::addDatabase is not re-entrant safe.
	SearchIndexStore::pauseBackgroundAccess();
	ui.setupUi(this);
	ui.labelBuildTime->setText(QStringLiteral("编译时间：") + QStringLiteral(__DATE__) + QStringLiteral(" ") + QStringLiteral(__TIME__));
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
	setupStatusBtn(ui.pbScreenshotStatus, ":/images/ok.png", QStringLiteral("快捷键注册状态"));
	setupStatusBtn(ui.pbPinStatus, ":/images/ok.png", QStringLiteral("快捷键注册状态"));
	setupStatusBtn(ui.pbChatStatus, ":/images/remove.png", QStringLiteral("清除对话窗口快捷键"));

    // 适配路径相关按钮
    int pathBtnWidth = Util::scaleSize(75);
    int pathBtnHeight = Util::scaleSize(25);
    ui.pbOpenImagePath->setMinimumSize(pathBtnWidth, pathBtnHeight);
    ui.pbOpenImagePath->setMaximumSize(pathBtnWidth, pathBtnHeight);
    ui.pbModifyImagePath->setMinimumSize(pathBtnWidth, pathBtnHeight);
    ui.pbModifyImagePath->setMaximumSize(pathBtnWidth, pathBtnHeight);

    // 适配恢复默认按钮
    ui.pbRevert->setMinimumSize(pathBtnWidth, pathBtnHeight);
    ui.pbRevert->setMaximumSize(pathBtnWidth, pathBtnHeight);
    QPixmap tipsPixmap(QString(":/images/tips.png"));
    ui.labelTips->setPixmap(tipsPixmap);
    ui.labelTips->setToolTip(QStringLiteral("在输入框上按下要设置的快捷键"));
    qInfo() << "Settings: initializing tabs...";
    initTablePath();
    qInfo() << "Settings: initTablePath done";
    initThemeSelector();
    qInfo() << "Settings: initThemeSelector done";
    initLlmTab();
    qInfo() << "Settings: initLlmTab done";
    initLocalSearchTab();
    qInfo() << "Settings: initLocalSearchTab done";
    applyModernLayout();
    qInfo() << "Settings: modern layout applied";

    // 使用系统原生标题栏（与 AI Fill Settings 一致），仅设置标题与关闭清理
    setAttribute(Qt::WA_DeleteOnClose, true);
    setWindowTitle(QStringLiteral("设置"));

    connect(ui.cbAutoStart, &QCheckBox::clicked, this, &Settings::onAutoStartClicked);
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
    connect(ui.sbNotifyWidth, QOverload<int>::of(&QSpinBox::valueChanged), this, &Settings::onChatWindowSettingChanged);
    connect(ui.sbNotifyHeight, QOverload<int>::of(&QSpinBox::valueChanged), this, &Settings::onChatWindowSettingChanged);
    connect(ui.cbNotifyPosition, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Settings::onChatWindowSettingChanged);
    connect(ui.kseScreenshot, &QKeySequenceEdit::editingFinished, this, &Settings::updateScreenshotGlobalKey);
    connect(ui.ksePin, &QKeySequenceEdit::editingFinished, this, &Settings::updatePinKey);
    connect(ui.kseChat, &QKeySequenceEdit::editingFinished, this, &Settings::updateChatKey);
    connect(ui.pbChatStatus, &QPushButton::clicked, this, &Settings::clearChatGlobalKey);
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

    QWidget* generalPage = pageByTitle(QStringLiteral("常规设置"));
    QWidget* imagePage = pageByTitle(QStringLiteral("图片"));
    QWidget* pathPage = pageByTitle(QStringLiteral("路径"));
    QWidget* llmHostPage = pageByTitle(QStringLiteral("大模型对话"));
    QWidget* githubPage = pageByTitle(QStringLiteral("GitHub图床"));
    QWidget* localSearchPage = pageByTitle(QStringLiteral("本地搜索"));
    QWidget* aboutPage = pageByTitle(QStringLiteral("关于"));
    QTabWidget* llmTabs = llmHostPage
        ? llmHostPage->findChild<QTabWidget*>(QString(), Qt::FindDirectChildrenOnly)
        : nullptr;

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
    navigation->addItems({QStringLiteral("常规"), QStringLiteral("截图与输出"),
                          QStringLiteral("本地搜索"), QStringLiteral("AI 助手"),
                          QStringLiteral("工具与划词"), QStringLiteral("路径记录"),
                          QStringLiteral("关于")});

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
            titleLabel->setText(QStringLiteral("常规"));
            subtitleLabel->setText(QStringLiteral("管理外观、启动和快捷键"));
            legacyTabs->setCurrentWidget(generalPage);
            break;
        case 1:
            titleLabel->setText(QStringLiteral("截图与输出"));
            subtitleLabel->setText(QStringLiteral("管理图片保存、OCR 与图床"));
            legacyTabs->setCurrentWidget(imagePage);
            subPages = {
                {QStringLiteral("图片与 OCR"), [=] { legacyTabs->setCurrentWidget(imagePage); }},
                {QStringLiteral("GitHub 图床"), [=] { legacyTabs->setCurrentWidget(githubPage); }}
            };
            break;
        case 2:
            titleLabel->setText(QStringLiteral("本地搜索"));
            subtitleLabel->setText(QStringLiteral("管理索引范围、排除规则和搜索引擎"));
            legacyTabs->setCurrentWidget(localSearchPage);
            break;
        case 3:
            titleLabel->setText(QStringLiteral("AI 助手"));
            subtitleLabel->setText(QStringLiteral("配置模型、网络搜索和对话窗口"));
            legacyTabs->setCurrentWidget(llmHostPage);
            if (llmTabs) {
                llmTabs->setCurrentIndex(0);
                for (int index : {0, 1, 3}) {
                    subPages.append({llmTabs->tabText(index),
                                     [=] { llmTabs->setCurrentIndex(index); }});
                }
            }
            break;
        case 4:
            titleLabel->setText(QStringLiteral("工具与划词"));
            subtitleLabel->setText(QStringLiteral("配置划词操作与 Agent 工具"));
            legacyTabs->setCurrentWidget(llmHostPage);
            if (llmTabs) {
                llmTabs->setCurrentIndex(2);
                for (int index : {2, 4}) {
                    subPages.append({llmTabs->tabText(index),
                                     [=] { llmTabs->setCurrentIndex(index); }});
                }
            }
            break;
        case 5:
            titleLabel->setText(QStringLiteral("路径记录"));
            subtitleLabel->setText(QStringLiteral("查看并复制常用保存路径"));
            legacyTabs->setCurrentWidget(pathPage);
            break;
        default:
            titleLabel->setText(QStringLiteral("关于"));
            subtitleLabel->setText(QStringLiteral("版本信息与项目说明"));
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
    ui.pbRevert->setFixedSize(Util::scaleSize(88), Util::scaleSize(34));
    ui.labelStatusTip->setObjectName(QStringLiteral("settingsStatusTip"));
    setWindowTitle(QStringLiteral("ntscreenshot 设置"));
}

void Settings::readData()
{
    qInfo() << "Settings::readData: start";
    SettingModel *setting = windowManager_->setting();
    ui.cbAutoStart->setCheckState(setting->autoStart() ? Qt::Checked : Qt::Unchecked);
    ui.cbAutoPin->setCheckState(setting->autoPin() ? Qt::Checked : Qt::Unchecked);
    ui.cbPinNoBorder->setCheckState(setting->pinNoBorder() ? Qt::Checked : Qt::Unchecked);
    ui.kseScreenshot->setKeySequence(QKeySequence::fromString(setting->screenhotGlobalKey(), QKeySequence::NativeText));
    ui.ksePin->setKeySequence(QKeySequence::fromString(setting->pinGlobalKey(), QKeySequence::NativeText));
    ui.kseChat->setKeySequence(QKeySequence::fromString(setting->chatGlobalKey(), QKeySequence::NativeText));
    setStatusIcon(ui.pbScreenshotStatus, !setting->screenhotGlobalKey().isEmpty());
    setStatusIcon(ui.pbPinStatus, !setting->pinGlobalKey().isEmpty());
    setStatusIcon(ui.pbChatStatus, !setting->chatGlobalKey().isEmpty());
    ui.rbRGB->setChecked(setting->rgbColor());
    ui.rbHexadecimal->setChecked(!setting->rgbColor());

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
    {
        const QSignalBlocker widthBlocker(ui.sbNotifyWidth);
        const QSignalBlocker heightBlocker(ui.sbNotifyHeight);
        const QSignalBlocker positionBlocker(ui.cbNotifyPosition);
        const QSize notifySize = setting->notificationWindowSize();
        ui.sbNotifyWidth->setValue(notifySize.width());
        ui.sbNotifyHeight->setValue(notifySize.height());
        ui.cbNotifyPosition->setCurrentIndex(static_cast<int>(setting->trayNotificationPosition()));
    }
    if (cbImageTokenSaving_) {
        const QSignalBlocker imageTokenSavingBlocker(cbImageTokenSaving_);
        cbImageTokenSaving_->setChecked(setting->llmImageTokenSavingEnabled());
    }

    qInfo() << "Settings::readData: loading providers and actions...";
    loadLlmProviders();
    loadNetworkSearchProviders();
    if (cbTextSelectionEnabled_) {
        const QSignalBlocker blocker(cbTextSelectionEnabled_);
        cbTextSelectionEnabled_->setChecked(setting->textSelectionEnabled());
    }
    loadTextSelectionActions();
    loadToolSettings();
    loadLocalSearchSettings();
    qInfo() << "Settings::readData: done";
}

void Settings::initLocalSearchTab()
{
    qInfo() << "Settings::initLocalSearchTab: start";
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    Util::scaleLayoutMargins(layout, 14, 14, 14, 14);
    layout->setSpacing(Util::scaleSize(8));

    auto* toolbar = new QFrame(page);
    toolbar->setFrameShape(QFrame::StyledPanel);
    auto* toolbarLayout = new QVBoxLayout(toolbar);
    Util::scaleLayoutMargins(toolbarLayout, 12, 7, 12, 7);
    toolbarLayout->setSpacing(Util::scaleSize(6));

    auto* shortcutLabel = new QLabel(QStringLiteral("快捷键"), toolbar);
    shortcutLabel->setToolTip(QStringLiteral("用于快速显示本地搜索窗口"));
    kseLocalSearch_ = new QKeySequenceEdit(toolbar);
    kseLocalSearch_->setFixedWidth(Util::scaleSize(170));
    kseLocalSearch_->setToolTip(
        QStringLiteral("默认 Alt+Space；快捷键冲突时可改为 Ctrl+Alt+Space"));
    auto* presetAltSpace = new QPushButton(QStringLiteral("Alt+Space"), toolbar);
    presetAltSpace->setToolTip(QStringLiteral("使用默认快捷键 Alt+Space"));

    auto* separator = new QFrame(toolbar);
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);

    auto* bookmarksLabel = new QLabel(QStringLiteral("书签"), toolbar);
    bookmarksLabel->setToolTip(QStringLiteral("启用后会将浏览器书签写入本地搜索索引"));
    cbLocalSearchChromeBookmarks_ = new QCheckBox(QStringLiteral("Chrome"), toolbar);
    cbLocalSearchEdgeBookmarks_ = new QCheckBox(QStringLiteral("Edge"), toolbar);
    cbLocalSearchChromeBookmarks_->setToolTip(QStringLiteral("索引 Chrome 书签"));
    cbLocalSearchEdgeBookmarks_->setToolTip(QStringLiteral("索引 Edge 书签"));

    auto* engineLabel = new QLabel(QStringLiteral("搜索引擎"), toolbar);
    cbLocalSearchWebEngine_ = new QComboBox(toolbar);
    cbLocalSearchWebEngine_->setFixedWidth(Util::scaleSize(270));
    cbLocalSearchWebEngine_->setToolTip(QStringLiteral("输入 < 前缀进行网页搜索时使用的搜索引擎"));
    const auto engines = SettingModel::localSearchWebEngines();
    for (const auto& engine : engines) {
        const QIcon icon = engine.iconPath.isEmpty() ? QIcon() : QIcon(engine.iconPath);
        cbLocalSearchWebEngine_->addItem(icon, engine.name, engine.id);
    }

    cbLocalSearchPinyin_ = new QCheckBox(QStringLiteral("拼音搜索"), toolbar);
    cbLocalSearchPinyin_->setToolTip(QStringLiteral("支持输入中文拼音检索文件/目录/应用"));

    auto* rebuild = new QPushButton(QStringLiteral("重建索引"), toolbar);
    rebuild->setToolTip(QStringLiteral("重新扫描目录、应用和已启用的浏览器书签"));

    labelLocalSearchStatus_ = new QLabel(toolbar);
    labelLocalSearchStatus_->setStyleSheet(
        QStringLiteral("color:%1;").arg(ThemeManager::tokens().textSecondary.name()));
    labelLocalSearchStatus_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    labelLocalSearchStatus_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // 第一行：快捷键 | 书签
    auto* toolbarRow0 = new QHBoxLayout();
    toolbarRow0->setSpacing(Util::scaleSize(8));
    toolbarRow0->addWidget(shortcutLabel);
    toolbarRow0->addWidget(kseLocalSearch_);
    toolbarRow0->addWidget(presetAltSpace);
    toolbarRow0->addSpacing(Util::scaleSize(10));
    toolbarRow0->addWidget(separator);
    toolbarRow0->addSpacing(Util::scaleSize(10));
    toolbarRow0->addWidget(bookmarksLabel);
    toolbarRow0->addWidget(cbLocalSearchChromeBookmarks_);
    toolbarRow0->addWidget(cbLocalSearchEdgeBookmarks_);
    toolbarRow0->addStretch();
    toolbarLayout->addLayout(toolbarRow0);

    // 第二行：搜索引擎 | 拼音搜索 | 重建索引
    auto* toolbarRow1 = new QHBoxLayout();
    toolbarRow1->setSpacing(Util::scaleSize(8));
    toolbarRow1->addWidget(engineLabel);
    toolbarRow1->addWidget(cbLocalSearchWebEngine_);
    toolbarRow1->addWidget(cbLocalSearchPinyin_);
    toolbarRow1->addStretch();
    toolbarRow1->addWidget(rebuild);
    toolbarLayout->addLayout(toolbarRow1);

    // 第三行：状态
    toolbarLayout->addWidget(labelLocalSearchStatus_);
    layout->addWidget(toolbar);

    // 检索目录 / 排除规则 用标签页分开，避免内容拥挤
    auto* listTabs = new QTabWidget(page);

    auto* rootsTab = new QWidget();
    auto* rootsLayout = new QVBoxLayout(rootsTab);
    Util::scaleLayoutMargins(rootsLayout, 12, 10, 12, 12);
    rootsLayout->setSpacing(Util::scaleSize(8));
    auto* rootsHint = new QLabel(
        QStringLiteral("仅添加经常需要搜索的位置；目录变化会在后台自动同步。"), rootsTab);
    rootsHint->setWordWrap(true);
    rootsHint->setStyleSheet(
        QStringLiteral("color:%1;").arg(ThemeManager::tokens().textSecondary.name()));
    rootsLayout->addWidget(rootsHint);
    auto* rootsRow = new QHBoxLayout();
    listLocalSearchRoots_ = new QListWidget(rootsTab);
    listLocalSearchRoots_->setSelectionMode(QAbstractItemView::SingleSelection);
    listLocalSearchRoots_->setAlternatingRowColors(true);
    listLocalSearchRoots_->setMinimumHeight(Util::scaleSize(200));
    rootsRow->addWidget(listLocalSearchRoots_, 1);
    auto* rootButtons = new QVBoxLayout();
    rootButtons->setSpacing(Util::scaleSize(6));
    auto* addRoot = new QPushButton(QStringLiteral("添加目录"), rootsTab);
    auto* removeRoot = new QPushButton(QStringLiteral("删除选中"), rootsTab);
    auto* restoreRoots = new QPushButton(QStringLiteral("恢复默认"), rootsTab);
    restoreRoots->setToolTip(QStringLiteral("恢复桌面、文档、下载、图片、音乐、视频和 OneDrive"));
    rootButtons->addWidget(addRoot);
    rootButtons->addWidget(removeRoot);
    rootButtons->addStretch();
    rootButtons->addWidget(restoreRoots);
    rootsRow->addLayout(rootButtons);
    rootsLayout->addLayout(rootsRow, 1);
    listTabs->addTab(rootsTab, QStringLiteral("索引目录"));

    auto* excludesTab = new QWidget();
    auto* excludesLayout = new QVBoxLayout(excludesTab);
    Util::scaleLayoutMargins(excludesLayout, 12, 10, 12, 12);
    excludesLayout->setSpacing(Util::scaleSize(8));
    auto* excludesHint = new QLabel(
        QStringLiteral("每行一条规则，按索引根目录的相对路径匹配。"),
        excludesTab);
    excludesHint->setWordWrap(true);
    excludesHint->setStyleSheet(
        QStringLiteral("color:%1;").arg(ThemeManager::tokens().textSecondary.name()));
    excludesLayout->addWidget(excludesHint);
    auto* excludesRow = new QHBoxLayout();
    teLocalSearchExcludes_ = new QPlainTextEdit(excludesTab);
    teLocalSearchExcludes_->setPlaceholderText(
        QStringLiteral("node_modules/\n**/*.tmp\n!important.tmp"));
    teLocalSearchExcludes_->setMinimumHeight(Util::scaleSize(200));
    teLocalSearchExcludes_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    excludesRow->addWidget(teLocalSearchExcludes_, 1);
    auto* rulesButtons = new QVBoxLayout();
    rulesButtons->setSpacing(Util::scaleSize(6));
    auto* saveRules = new QPushButton(QStringLiteral("保存"), excludesTab);
    auto* resetRules = new QPushButton(QStringLiteral("恢复默认"), excludesTab);
    resetRules->setToolTip(QStringLiteral("恢复后点击\"保存\"以应用"));
    rulesButtons->addWidget(saveRules);
    rulesButtons->addStretch();
    rulesButtons->addWidget(resetRules);
    excludesRow->addLayout(rulesButtons);
    excludesLayout->addLayout(excludesRow, 1);
    listTabs->addTab(excludesTab, QStringLiteral("排除规则"));

    layout->addWidget(listTabs, 1);

    const int aboutIndex = ui.tabWidget->indexOf(ui.tab_2);
    if (aboutIndex >= 0) {
        ui.tabWidget->insertTab(aboutIndex, page, QStringLiteral("本地搜索"));
    } else {
        ui.tabWidget->addTab(page, QStringLiteral("本地搜索"));
    }

    const auto applyLocalSearchHotkey = [this](const QString& key) {
        if (windowManager_->setLocalSearchGlobalKey(key)) {
            windowManager_->setting()->setLocalSearchGlobalKey(key);
            emit windowManager_->sigSettingChanged();
            showStatusTip(QStringLiteral("本地搜索快捷键已更新"));
            return;
        }

        const QString reason = windowManager_->lastHotkeyError();
        const QString detail = reason.isEmpty()
            ? QStringLiteral("快捷键注册失败，可能被占用")
            : QStringLiteral("快捷键注册失败：%1").arg(reason);
        showStatusTip(detail, false);
        QMessageBox::warning(this, QStringLiteral("快捷键冲突"),
                             detail + QStringLiteral("\n请更换组合键。"));
        kseLocalSearch_->setKeySequence(QKeySequence::fromString(
            windowManager_->setting()->localSearchGlobalKey(), QKeySequence::NativeText));
    };
    connect(kseLocalSearch_, &QKeySequenceEdit::editingFinished, this,
            [this, applyLocalSearchHotkey]() {
        applyLocalSearchHotkey(keySequenceText(kseLocalSearch_->keySequence()));
    });
    connect(presetAltSpace, &QPushButton::clicked, this, [this, applyLocalSearchHotkey]() {
        const QString key = QStringLiteral("Alt+Space");
        kseLocalSearch_->setKeySequence(QKeySequence::fromString(
            key, QKeySequence::NativeText));
        applyLocalSearchHotkey(key);
    });
    const auto saveBookmarkSources = [this]() {
        QStringList sources;
        if (cbLocalSearchChromeBookmarks_->isChecked()) sources.push_back(QStringLiteral("chrome"));
        if (cbLocalSearchEdgeBookmarks_->isChecked()) sources.push_back(QStringLiteral("edge"));
        windowManager_->setting()->setLocalSearchBookmarkSources(sources);
        emit windowManager_->sigSettingChanged();
        showStatusTip(QStringLiteral("书签来源已更新，索引将自动重建"));
    };
    connect(cbLocalSearchChromeBookmarks_, &QCheckBox::toggled,
            this, [saveBookmarkSources](bool) { saveBookmarkSources(); });
    connect(cbLocalSearchEdgeBookmarks_, &QCheckBox::toggled,
            this, [saveBookmarkSources](bool) { saveBookmarkSources(); });
    connect(cbLocalSearchWebEngine_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() {
        const QString id = cbLocalSearchWebEngine_->currentData().toString();
        windowManager_->setting()->setLocalSearchWebEngine(id);
        emit windowManager_->sigSettingChanged();
        showStatusTip(QStringLiteral("搜索引擎已更新"));
    });
    connect(cbLocalSearchPinyin_, &QCheckBox::toggled, this, [this](bool checked) {
        windowManager_->setting()->setLocalSearchPinyinEnabled(checked);
        emit windowManager_->sigSettingChanged();
        showStatusTip(checked ? QStringLiteral("已启用拼音搜索")
                              : QStringLiteral("已关闭拼音搜索"));
    });
    connect(addRoot, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("选择索引目录"));
        if (path.isEmpty()) return;
        QStringList roots = windowManager_->setting()->localSearchRoots();
        if (!roots.contains(path, Qt::CaseInsensitive)) roots.push_back(path);
        windowManager_->setting()->setLocalSearchRoots(roots);
        loadLocalSearchSettings();
        emit windowManager_->sigSettingChanged();
        showStatusTip(QStringLiteral("已添加索引目录"));
    });
    connect(removeRoot, &QPushButton::clicked, this, [this]() {
        const int row = listLocalSearchRoots_->currentRow();
        if (row < 0) return;
        QStringList roots = windowManager_->setting()->localSearchRoots();
        roots.removeAt(row);
        windowManager_->setting()->setLocalSearchRoots(roots);
        loadLocalSearchSettings();
        emit windowManager_->sigSettingChanged();
        showStatusTip(QStringLiteral("已移除索引目录"));
    });
    connect(restoreRoots, &QPushButton::clicked, this, [this]() {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("恢复默认"),
            QStringLiteral("将用默认目录替换当前索引目录，是否继续？"));
        if (answer != QMessageBox::Yes) return;
        windowManager_->setting()->setLocalSearchRoots(
            SettingModel::defaultLocalSearchRoots());
        loadLocalSearchSettings();
        emit windowManager_->sigSettingChanged();
        showStatusTip(QStringLiteral("已恢复推荐索引目录"));
    });

    const auto savePatterns = [this]() -> bool {
        QStringList patterns;
        for (const QString& line : teLocalSearchExcludes_->toPlainText().split('\n')) {
            const QString value = line.trimmed();
            if (value.isEmpty()) {
                patterns.push_back(QString());
                continue;
            }
            if (!value.startsWith(u'#')) {
                QString error;
                if (!IgnorePatternMatcher::validate(value, &error)) {
                    showStatusTip(QStringLiteral("排除规则无效"), false);
                    QMessageBox::warning(this, QStringLiteral("排除规则无效"),
                                         QStringLiteral("%1\n%2").arg(value, error));
                    return false;
                }
            }
            patterns.push_back(value);
        }
        while (!patterns.isEmpty() && patterns.constLast().isEmpty()) patterns.removeLast();
        windowManager_->setting()->setLocalSearchExcludePatterns(patterns);
        emit windowManager_->sigSettingChanged();
        return true;
    };
    connect(saveRules, &QPushButton::clicked, this, [this, savePatterns]() {
        if (savePatterns()) showStatusTip(QStringLiteral("排除规则已保存，索引将自动更新"));
    });
    connect(resetRules, &QPushButton::clicked, this, [this]() {
        teLocalSearchExcludes_->setPlainText(
            SettingModel::defaultLocalSearchExcludePatterns().join('\n'));
        teLocalSearchExcludes_->setFocus();
        showStatusTip(QStringLiteral("已恢复默认排除规则，点击保存生效"));
    });
    connect(rebuild, &QPushButton::clicked, this, [this, savePatterns]() {
        if (!savePatterns()) return;
        windowManager_->rebuildLocalSearchIndex();
        labelLocalSearchStatus_->setText(QStringLiteral("已请求重建…"));
        showStatusTip(QStringLiteral("索引重建已开始"));
    });

    auto* statusTimer = new QTimer(page);
    statusTimer->setInterval(1000);
    connect(statusTimer, &QTimer::timeout, this, [this]() {
        if (labelLocalSearchStatus_) labelLocalSearchStatus_->setText(windowManager_->localSearchStatus());
    });
    statusTimer->start();
    qInfo() << "Settings::initLocalSearchTab: done";
}

void Settings::loadLocalSearchSettings()
{
    if (!listLocalSearchRoots_ || !cbLocalSearchWebEngine_) return;
    listLocalSearchRoots_->clear();
    listLocalSearchRoots_->addItems(windowManager_->setting()->localSearchRoots());
    teLocalSearchExcludes_->setPlainText(
        windowManager_->setting()->localSearchExcludePatterns().join('\n'));
    kseLocalSearch_->setKeySequence(QKeySequence::fromString(
        windowManager_->setting()->localSearchGlobalKey(), QKeySequence::NativeText));
    labelLocalSearchStatus_->setText(windowManager_->localSearchStatus());
    const QStringList bookmarkSources = windowManager_->setting()->localSearchBookmarkSources();
    const QSignalBlocker chromeBlocker(cbLocalSearchChromeBookmarks_);
    const QSignalBlocker edgeBlocker(cbLocalSearchEdgeBookmarks_);
    cbLocalSearchChromeBookmarks_->setChecked(bookmarkSources.contains(QStringLiteral("chrome")));
    cbLocalSearchEdgeBookmarks_->setChecked(bookmarkSources.contains(QStringLiteral("edge")));
    const QString webEngine = windowManager_->setting()->localSearchWebEngine();
    const int engineIndex = cbLocalSearchWebEngine_->findData(webEngine);
    const QSignalBlocker engineBlocker(cbLocalSearchWebEngine_);
    cbLocalSearchWebEngine_->setCurrentIndex(engineIndex >= 0 ? engineIndex : 0);
    if (cbLocalSearchPinyin_) {
        const QSignalBlocker pinyinBlocker(cbLocalSearchPinyin_);
        cbLocalSearchPinyin_->setChecked(
            windowManager_->setting()->localSearchPinyinEnabled());
    }
}

void Settings::initTablePath()
{
    const QList<QPair<QString, QString>> data = {
        {QStringLiteral("程序"), Util::getRunDir()},
        {QStringLiteral("配置"), Util::getConfigDir()},
        {QStringLiteral("日志"), Util::getLogsDir()}
    };

    ui.tablePath->setFrameShape(QFrame::NoFrame);
    ui.tablePath->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui.tablePath->setAlternatingRowColors(true);
    ui.tablePath->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui.tablePath->verticalHeader()->setVisible(false);
    ui.tablePath->horizontalHeader()->setVisible(false);
    ui.tablePath->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui.tablePath->setFocusPolicy(Qt::NoFocus);
    ui.tablePath->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui.tablePath->setShowGrid(false);
    ui.tablePath->setMouseTracking(true);
    ui.tablePath->horizontalHeader()->setStretchLastSection(true);
    ui.tablePath->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui.tablePath->setRowCount(data.size());
    ui.tablePath->setColumnCount(2);
    ui.tablePath->setTextElideMode(Qt::ElideMiddle);

    connect(ui.tablePath, &QTableWidget::cellDoubleClicked, this, &Settings::onTablePathDoubleClicked);
    for (int row = 0; row < data.size(); row++) {
        ui.tablePath->setItem(row, 0, new QTableWidgetItem(data[row].first));
        ui.tablePath->setItem(row, 1, new QTableWidgetItem(data[row].second));
    }
}

void Settings::initThemeSelector()
{
    QHBoxLayout *themeLayout = new QHBoxLayout();
    QLabel *themeLabel = new QLabel(QStringLiteral("皮肤:"), this);
    cbTheme_ = new QComboBox(this);
    cbTheme_->addItem(QStringLiteral("黑色"), static_cast<int>(AppTheme::Dark));
    cbTheme_->addItem(QStringLiteral("白色"), static_cast<int>(AppTheme::Light));
    themeLayout->addWidget(themeLabel);
    themeLayout->addWidget(cbTheme_);
    themeLayout->addStretch();

    ui.verticalLayout_2->insertLayout(1, themeLayout);

    connect(cbTheme_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Settings::onThemeChanged);
}

void Settings::updateScreenshotGlobalKey()
{
    const QString key = keySequenceText(ui.kseScreenshot->keySequence());
    if (!key.isEmpty() && windowManager_->setScreenshotGlobalKey(key)) {
        windowManager_->setting()->setScreenshotGlobalKey(key);
        setStatusIcon(ui.pbScreenshotStatus, true);
        emit windowManager_->sigSettingChanged();
        showStatusTip(QStringLiteral("截图快捷键已更新"));
    } else {
        setStatusIcon(ui.pbScreenshotStatus, false);
        if (key.isEmpty()) {
            showStatusTip(QStringLiteral("快捷键不能为空"), false);
        } else {
            const QString reason = windowManager_->lastHotkeyError();
            showStatusTip(reason.isEmpty()
                              ? QStringLiteral("快捷键注册失败，可能被占用")
                              : QStringLiteral("快捷键注册失败：%1").arg(reason),
                          false);
        }
    }
}

void Settings::updatePinKey()
{
    const QString key = keySequenceText(ui.ksePin->keySequence());
    if (!key.isEmpty() && windowManager_->setPinGlobalKey(key)) {
        windowManager_->setting()->setPinGlobalKey(key);
        setStatusIcon(ui.pbPinStatus, true);
        emit windowManager_->sigSettingChanged();
        showStatusTip(QStringLiteral("贴图快捷键已更新"));
    } else {
        setStatusIcon(ui.pbPinStatus, false);
        if (key.isEmpty()) {
            showStatusTip(QStringLiteral("快捷键不能为空"), false);
        } else {
            const QString reason = windowManager_->lastHotkeyError();
            showStatusTip(reason.isEmpty()
                              ? QStringLiteral("快捷键注册失败，可能被占用")
                              : QStringLiteral("快捷键注册失败：%1").arg(reason),
                          false);
        }
    }
}

void Settings::updateChatKey()
{
    const QString key = keySequenceText(ui.kseChat->keySequence());
    if (!key.isEmpty() && windowManager_->setChatGlobalKey(key)) {
        windowManager_->setting()->setChatGlobalKey(key);
        setStatusIcon(ui.pbChatStatus, true);
        emit windowManager_->sigSettingChanged();
        showStatusTip(QStringLiteral("对话快捷键已更新"));
    } else if (key.isEmpty()) {
        clearChatGlobalKey();
    } else {
        setStatusIcon(ui.pbChatStatus, false);
        const QString reason = windowManager_->lastHotkeyError();
        showStatusTip(reason.isEmpty()
                          ? QStringLiteral("快捷键注册失败，可能被占用")
                          : QStringLiteral("快捷键注册失败：%1").arg(reason),
                      false);
    }
}

void Settings::clearChatGlobalKey()
{
    windowManager_->setChatGlobalKey(QString());
    windowManager_->setting()->setChatGlobalKey(QString());
    ui.kseChat->setKeySequence(QKeySequence());
    setStatusIcon(ui.pbChatStatus, false);
    emit windowManager_->sigSettingChanged();
    showStatusTip(QStringLiteral("对话快捷键已清除"));
}

void Settings::onAutoStartClicked(bool checked)
{
    windowManager_->setting()->setAutoStart(checked);
    emit windowManager_->sigSettingChanged();
    showStatusTip(checked ? QStringLiteral("已开启开机自启动") : QStringLiteral("已关闭开机自启动"));
}

void Settings::onAutoPin(bool checked)
{
    windowManager_->setting()->setAutoPin(checked);
    emit windowManager_->sigSettingChanged();
    showStatusTip(checked ? QStringLiteral("已开启自动贴图") : QStringLiteral("已关闭自动贴图"));
}

void Settings::onPinNoBorder(bool checked)
{
    windowManager_->setting()->setPinNoBorder(checked);
    emit windowManager_->sigSettingChanged();
    showStatusTip(checked ? QStringLiteral("已开启贴图无边框") : QStringLiteral("已关闭贴图无边框"));
}

void Settings::onRevertClicked()
{
    windowManager_->setting()->revertDefault();
    readData();
    CDarkStyle::assign(windowManager_->setting()->themeMode());
    emit windowManager_->sigSettingChanged();
    showStatusTip(QStringLiteral("已恢复默认设置"));
}

void Settings::onTablePathDoubleClicked(int row, int col)
{
    QString text = ui.tablePath->item(row, col)->text();
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(text);
}

void Settings::onRgbColorToggled(bool checked)
{
	windowManager_->setting()->setRgbColor(checked);
}

void Settings::onHexColorToggled(bool checked)
{
	windowManager_->setting()->setRgbColor(!checked);
}

void Settings::onAutoSaveChanged()
{
    bool autoSave = ui.cbAutoSave->isChecked();
    ui.pbModifyImagePath->setEnabled(autoSave);
    windowManager_->setting()->setAutoSaveImage(autoSave, ui.leSavePath->text().trimmed());
    showStatusTip(autoSave ? QStringLiteral("已开启自动保存") : QStringLiteral("已关闭自动保存"));
}

void Settings::onOpenImagePath()
{
	Util::shellExecute(ui.leSavePath->text().trimmed());
}

void Settings::onModifyImagePath()
{
	QString newDir = QFileDialog::getExistingDirectory(nullptr, QStringLiteral("选择截图保存目录"));
	if (!newDir.isEmpty()) {
		ui.leSavePath->setText(newDir);
		onAutoSaveChanged();
		showStatusTip(QStringLiteral("保存路径已更新"));
	}
}

void Settings::onBackgroundChanged()
{
    ui.hsBackgroundAlpha->setEnabled(ui.cbBackground->isChecked());
    if (!ui.cbBackground->isChecked()) {
        ui.hsBackgroundAlpha->setValue(160);
    }
    windowManager_->setting()->setBackgroundColor(ui.cbBackground->isChecked(), ui.hsBackgroundAlpha->value());
    showStatusTip(QStringLiteral("截图背景设置已更新"));
}

void Settings::onBackgroundAlphaReleased()
{
    windowManager_->setting()->setBackgroundColor(ui.cbBackground->isChecked(), ui.hsBackgroundAlpha->value());
    showStatusTip(QStringLiteral("背景透明度已更新"));
}

void Settings::onThemeChanged(int index)
{
    if (!cbTheme_ || index < 0) {
        return;
    }

    const QVariant data = cbTheme_->itemData(index);
    if (!data.isValid()) {
        return;
    }

    const AppTheme themeMode = static_cast<AppTheme>(data.toInt());
    SettingModel *setting = windowManager_->setting();
    if (setting->themeMode() == themeMode) {
        return;
    }

    setting->setThemeMode(themeMode);
    CDarkStyle::assign(themeMode);
    emit windowManager_->sigSettingChanged();
    showStatusTip(themeMode == AppTheme::Dark ? QStringLiteral("已切换为黑色主题") : QStringLiteral("已切换为白色主题"));
}

void Settings::initLlmTab()
{
    qInfo() << "Settings::initLlmTab: start";
    QTabWidget* tabWidget = new QTabWidget(ui.tab_llm);

    // Keep Designer-owned widgets in their original hierarchy. Moving them
    // between layouts and then dismantling the old form caused dangling
    // QLayoutItems. The old form is hidden; the active page owns fresh fields.
    const QList<QWidget*> designerLlmWidgets{
        ui.label_llm_base_url, ui.leLlmApiBaseUrl,
        ui.label_llm_api_key, ui.leLlmApiKey,
        ui.label_llm_model, ui.leLlmModel,
        ui.label_llm_temperature, ui.dsbLlmTemperature
    };
    for (QWidget* widget : designerLlmWidgets) {
        if (widget) widget->hide();
    }
    ui.verticalSpacer_llm->changeSize(0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum);
    ui.verticalLayout_llm->invalidate();

    // --- Tab 1: Provider Settings ---
    QWidget* generalPage = new QWidget();
    QVBoxLayout* generalVLayout = new QVBoxLayout(generalPage);
    generalVLayout->setContentsMargins(10, 10, 10, 10);
    generalVLayout->setSpacing(8);

    // Provider selector row
    QHBoxLayout* selectorRow = new QHBoxLayout();
    selectorRow->setSpacing(5);
    cbLlmProviders_ = new QComboBox(generalPage);
    int smallBtnW = Util::scaleSize(50);
    int smallBtnH = Util::scaleSize(25);
    QPushButton* btnAddProvider = new QPushButton(QStringLiteral("新增"), generalPage);
    btnAddProvider->setFixedSize(smallBtnW, smallBtnH);
    QPushButton* btnDelProvider = new QPushButton(QStringLiteral("删除"), generalPage);
    btnDelProvider->setFixedSize(smallBtnW, smallBtnH);
    selectorRow->addWidget(cbLlmProviders_, 1);
    selectorRow->addWidget(btnAddProvider);
    selectorRow->addWidget(btnDelProvider);

    QFormLayout* generalLayout = new QFormLayout();
    generalLayout->setSpacing(8);
    leLlmProviderName_ = new QLineEdit(generalPage);
    ui.leLlmApiBaseUrl = new QLineEdit(generalPage);
    ui.leLlmApiBaseUrl->setPlaceholderText(QStringLiteral("https://api.xxx.com/v1"));
    ui.leLlmApiKey = new QLineEdit(generalPage);
    ui.leLlmApiKey->setEchoMode(QLineEdit::Password);
    ui.leLlmApiKey->setPlaceholderText(QStringLiteral("请输入 API Key"));
    ui.leLlmModel = new QLineEdit(generalPage);
    ui.leLlmModel->setPlaceholderText(QStringLiteral("例如：gpt-4o-mini"));
    ui.dsbLlmTemperature = new QDoubleSpinBox(generalPage);
    ui.dsbLlmTemperature->setRange(0.0, 2.0);
    ui.dsbLlmTemperature->setSingleStep(0.1);
    ui.dsbLlmTemperature->setDecimals(2);
    generalLayout->addRow(QStringLiteral("大模型厂商:"),    selectorRow);
    generalLayout->addRow(QStringLiteral("名称:"),         leLlmProviderName_);
    generalLayout->addRow(QStringLiteral("API Base URL:"), ui.leLlmApiBaseUrl);
    generalLayout->addRow(QStringLiteral("API Key:"),      ui.leLlmApiKey);
    generalLayout->addRow(QStringLiteral("Model:"),        ui.leLlmModel);
    generalLayout->addRow(QStringLiteral("Temperature:"),  ui.dsbLlmTemperature);
    generalVLayout->addLayout(generalLayout);
    cbImageTokenSaving_ = new QCheckBox(QStringLiteral("图片省token"), generalPage);
    cbImageTokenSaving_->setChecked(true);
    generalVLayout->addWidget(cbImageTokenSaving_);
    connect(cbImageTokenSaving_, &QCheckBox::toggled, this, &Settings::onImageTokenSavingToggled);
    generalVLayout->addStretch();
    qInfo() << "Settings::initLlmTab: provider page built";

    QWidget* networkSearchPage = new QWidget();
    QVBoxLayout* networkSearchPanelLayout = new QVBoxLayout(networkSearchPage);
    networkSearchPanelLayout->setContentsMargins(10, 10, 10, 10);
    networkSearchPanelLayout->setSpacing(8);

    cbWebSearchEnabled_ = new QCheckBox(QStringLiteral("启用网络搜索"), networkSearchPage);
    networkSearchPanelLayout->addWidget(cbWebSearchEnabled_);

    QHBoxLayout* networkSelectorRow = new QHBoxLayout();
    networkSelectorRow->setSpacing(5);
    cbNetworkSearchProviders_ = new QComboBox(networkSearchPage);
    cbNetworkSearchProviders_->addItem(QStringLiteral("Tavily"), QStringLiteral("tavily"));
    networkSelectorRow->addWidget(cbNetworkSearchProviders_, 1);

    QFormLayout* networkSearchLayout = new QFormLayout();
    networkSearchLayout->setSpacing(8);
    leNetworkSearchApiKey_ = new QLineEdit(networkSearchPage);
    leNetworkSearchApiKey_->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    networkSearchLayout->addRow(QStringLiteral("搜索服务商:"), networkSelectorRow);
    networkSearchLayout->addRow(QStringLiteral("API KEY:"), leNetworkSearchApiKey_);
    networkSearchPanelLayout->addLayout(networkSearchLayout);

    QLabel *networkSearchHint = new QLabel(
        QStringLiteral("目前仅支持 Tavily，调用参数由程序内置：search_depth=advanced。"),
        networkSearchPage);
    networkSearchHint->setWordWrap(true);
    networkSearchHint->setStyleSheet(
        QStringLiteral("color:%1; font-size:12px;").arg(ThemeManager::tokens().textSecondary.name()));
    networkSearchPanelLayout->addWidget(networkSearchHint);
    networkSearchPanelLayout->addStretch();

    connect(cbLlmProviders_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Settings::onLlmProviderSelected);
    connect(btnAddProvider, &QPushButton::clicked, this, &Settings::onAddLlmProvider);
    connect(btnDelProvider, &QPushButton::clicked, this, &Settings::onDelLlmProvider);
    connect(leLlmProviderName_, &QLineEdit::textChanged,   this, &Settings::onLlmProviderFieldChanged);
    connect(ui.leLlmApiBaseUrl, &QLineEdit::textChanged,   this, &Settings::onLlmProviderFieldChanged);
    connect(ui.leLlmApiKey,     &QLineEdit::textChanged,   this, &Settings::onLlmProviderFieldChanged);
    connect(ui.leLlmModel,      &QLineEdit::textChanged,   this, &Settings::onLlmProviderFieldChanged);
    connect(ui.dsbLlmTemperature, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &Settings::onLlmProviderFieldChanged);
    connect(cbWebSearchEnabled_, &QCheckBox::toggled,
            this, &Settings::onWebSearchEnabledToggled);
    connect(cbNetworkSearchProviders_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Settings::onNetworkSearchProviderSelected);
    connect(leNetworkSearchApiKey_, &QLineEdit::textChanged,
            this, &Settings::onNetworkSearchProviderFieldChanged);
    qInfo() << "Settings::initLlmTab: network page built";

    // --- Tab 2: Text Selection Actions ---
    QWidget* textSelectionPage = new QWidget();
    QVBoxLayout* textSelectionLayout = new QVBoxLayout(textSelectionPage);
    textSelectionLayout->setContentsMargins(10, 10, 10, 10);
    textSelectionLayout->setSpacing(8);

    cbTextSelectionEnabled_ = new QCheckBox(QStringLiteral("启用划词功能"), textSelectionPage);
    textSelectionLayout->addWidget(cbTextSelectionEnabled_);

    textSelectionLayout->addWidget(new QLabel(QStringLiteral("配置划词工具栏按钮及对应 Prompt："), textSelectionPage));
    auto *textSelectionHint = new QLabel(QStringLiteral("新增或修改后，下次划词弹出工具栏时立即生效。"), textSelectionPage);
    textSelectionHint->setStyleSheet(
        QStringLiteral("color:%1; font-size:12px;").arg(ThemeManager::tokens().textSecondary.name()));
    textSelectionLayout->addWidget(textSelectionHint);

    QHBoxLayout* textSelectionContentRow = new QHBoxLayout();
    textSelectionContentRow->setSpacing(10);
    textSelectionContentRow->setAlignment(Qt::AlignTop);

    QVBoxLayout* textSelectionLeftLayout = new QVBoxLayout();
    textSelectionLeftLayout->setContentsMargins(0, 0, 0, 0);
    textSelectionLeftLayout->setSpacing(6);
    listTextSelectionActions_ = new QListWidget(textSelectionPage);
    listTextSelectionActions_->setAlternatingRowColors(true);
    listTextSelectionActions_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listTextSelectionActions_->setTextElideMode(Qt::ElideRight);
    listTextSelectionActions_->setWordWrap(false);
    listTextSelectionActions_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    const int actionTextWidth = listTextSelectionActions_->fontMetrics().horizontalAdvance(
        QStringLiteral("按钮文字字"));
    const int actionListWidth = actionTextWidth
        + Util::scaleSize(28)
        + listTextSelectionActions_->frameWidth() * 2
        + style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    listTextSelectionActions_->setFixedWidth(actionListWidth);
    textSelectionLeftLayout->addWidget(listTextSelectionActions_, 1);

    QVBoxLayout* textSelectionButtonLayout = new QVBoxLayout();
    textSelectionButtonLayout->setContentsMargins(0, 0, 0, 0);
    textSelectionButtonLayout->setSpacing(6);
    QPushButton* btnAddTextSelectionAction = new QPushButton(QStringLiteral("新增"), textSelectionPage);
    btnAddTextSelectionAction->setFixedSize(smallBtnW, smallBtnH);
    btnDelTextSelectionAction_ = new QPushButton(QStringLiteral("删除"), textSelectionPage);
    btnDelTextSelectionAction_->setFixedSize(smallBtnW, smallBtnH);
    textSelectionButtonLayout->addWidget(btnAddTextSelectionAction);
    textSelectionButtonLayout->addWidget(btnDelTextSelectionAction_);
    textSelectionButtonLayout->addStretch();

    QWidget* textSelectionEditor = new QWidget(textSelectionPage);
    QVBoxLayout* textSelectionForm = new QVBoxLayout(textSelectionEditor);
    textSelectionForm->setContentsMargins(0, 0, 0, 2);
    textSelectionForm->setSpacing(8);
    leTextSelectionActionLabel_ = new QLineEdit(textSelectionEditor);
    leTextSelectionActionLabel_->setPlaceholderText(QStringLiteral("按钮文字"));
    teTextSelectionActionPrompt_ = new QPlainTextEdit(textSelectionEditor);
    teTextSelectionActionPrompt_->setPlaceholderText(QStringLiteral("Prompt"));
    teTextSelectionActionPrompt_->setMinimumHeight(Util::scaleSize(120));
    teTextSelectionActionPrompt_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    teTextSelectionActionPrompt_->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    textSelectionForm->addWidget(leTextSelectionActionLabel_);
    textSelectionForm->addWidget(teTextSelectionActionPrompt_, 1);
    textSelectionEditor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    textSelectionContentRow->addLayout(textSelectionLeftLayout, 0);
    textSelectionContentRow->addLayout(textSelectionButtonLayout, 0);
    textSelectionContentRow->addWidget(textSelectionEditor, 1);
    textSelectionLayout->addLayout(textSelectionContentRow, 1);

    connect(listTextSelectionActions_, &QListWidget::currentRowChanged,
            this, &Settings::onTextSelectionActionSelected);
    connect(btnAddTextSelectionAction, &QPushButton::clicked,
            this, &Settings::onAddTextSelectionAction);
    connect(btnDelTextSelectionAction_, &QPushButton::clicked,
            this, &Settings::onDelTextSelectionAction);
    connect(leTextSelectionActionLabel_, &QLineEdit::textChanged,
            this, &Settings::onTextSelectionActionFieldChanged);
    connect(teTextSelectionActionPrompt_, &QPlainTextEdit::textChanged,
            this, &Settings::onTextSelectionActionFieldChanged);
    connect(cbTextSelectionEnabled_, &QCheckBox::toggled,
            this, &Settings::onTextSelectionEnabledToggled);
    qInfo() << "Settings::initLlmTab: selection page built";

    QWidget* notificationPage = new QWidget();
    QVBoxLayout* notificationPageLayout = new QVBoxLayout(notificationPage);
    notificationPageLayout->setContentsMargins(10, 10, 10, 10);
    notificationPageLayout->setSpacing(8);
    auto* notificationForm = new QFormLayout();
    auto* notificationSizeRow = new QHBoxLayout();
    ui.sbNotifyWidth = new QSpinBox(notificationPage);
    ui.sbNotifyWidth->setMinimum(420);
    ui.sbNotifyWidth->setMaximum(1200);
    ui.sbNotifyHeight = new QSpinBox(notificationPage);
    ui.sbNotifyHeight->setMinimum(300);
    ui.sbNotifyHeight->setMaximum(900);
    notificationSizeRow->addWidget(ui.sbNotifyWidth);
    notificationSizeRow->addWidget(new QLabel(QStringLiteral("×"), notificationPage));
    notificationSizeRow->addWidget(ui.sbNotifyHeight);
    notificationSizeRow->addStretch();
    notificationForm->addRow(QStringLiteral("窗口大小："), notificationSizeRow);
    ui.cbNotifyPosition = new QComboBox(notificationPage);
    ui.cbNotifyPosition->addItems({QStringLiteral("左上"), QStringLiteral("右上"),
                                   QStringLiteral("左下"), QStringLiteral("右下")});
    notificationForm->addRow(QStringLiteral("展示位置："), ui.cbNotifyPosition);
    notificationPageLayout->addLayout(notificationForm);
    notificationPageLayout->addStretch();
    qInfo() << "Settings::initLlmTab: notification page built";

    tabWidget->addTab(generalPage, QStringLiteral("厂商配置"));
    tabWidget->addTab(networkSearchPage, QStringLiteral("网络搜索"));
    tabWidget->addTab(textSelectionPage, QStringLiteral("划词设置"));
    tabWidget->addTab(notificationPage, QStringLiteral("对话窗口"));
    qInfo() << "Settings::initLlmTab: first tabs added";

    // --- Tab 4: Tool Management ---
    QWidget* toolPage = new QWidget();
    QVBoxLayout* toolPageLayout = new QVBoxLayout(toolPage);
    toolPageLayout->setContentsMargins(10, 10, 10, 10);
    toolPageLayout->setSpacing(8);

    toolPageLayout->addWidget(new QLabel(QStringLiteral("勾选启用 Agent 可使用的工具："), toolPage));

    listToolItems_ = new QListWidget(toolPage);
    listToolItems_->setAlternatingRowColors(true);
    listToolItems_->setUniformItemSizes(false);
    listToolItems_->setSelectionMode(QAbstractItemView::NoSelection);
    listToolItems_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    listToolItems_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listToolItems_->setWordWrap(true);
    listToolItems_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    const QList<LlmTools::ToolMeta> allToolMeta = LlmTools::builtinToolMetas();
    qInfo() << "Settings::initLlmTab: tool metadata loaded" << allToolMeta.size();
    for (const auto &meta : allToolMeta) {
        auto *item = new QListWidgetItem(
            QStringLiteral("%1: %2").arg(meta.name, meta.description),
            listToolItems_);
        item->setData(Qt::UserRole, meta.name);
        item->setToolTip(meta.description);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        item->setCheckState(Qt::Unchecked);
    }
    connect(listToolItems_, &QListWidget::itemChanged, this, [this](QListWidgetItem *) {
        onToolCheckChanged();
    });
    toolPageLayout->addWidget(listToolItems_, 1);

    cbToolAutoPermission_ = new QCheckBox(QStringLiteral("自动授权（执行工具时不再弹出确认）"), toolPage);
    cbToolAutoPermission_->setChecked(false);
    connect(cbToolAutoPermission_, &QCheckBox::toggled, this, &Settings::onToolAutoPermissionToggled);
    toolPageLayout->addWidget(cbToolAutoPermission_);

    tabWidget->addTab(toolPage, QStringLiteral("工具管理"));
    qInfo() << "Settings::initLlmTab: tool tab added";

    const int notificationTabIndex = ui.tabWidget->indexOf(ui.tab_notification);
    if (notificationTabIndex >= 0) {
        ui.tabWidget->removeTab(notificationTabIndex);
    }

    ui.verticalLayout_llm->addWidget(tabWidget, 1);
    qInfo() << "Settings::initLlmTab: done";
}

void Settings::loadLlmProviders()
{
    updatingProviderFields_ = true;

    auto *setting  = windowManager_->setting();
    const auto providers   = setting->llmProviders();
    const int  activeIndex = qBound(0, setting->llmActiveProviderIndex(), providers.size() - 1);

    cbLlmProviders_->clear();
    for (const auto &p : providers) {
        cbLlmProviders_->addItem(p.name.isEmpty() ? QStringLiteral("未命名") : p.name);
    }
    cbLlmProviders_->setCurrentIndex(activeIndex);

    if (!providers.isEmpty()) {
        const LlmProviderConfig &p = providers.at(activeIndex);
        leLlmProviderName_->setText(p.name);
        ui.leLlmApiBaseUrl->setText(p.apiBaseUrl);
        ui.leLlmApiKey->setText(p.apiKey);
        ui.leLlmModel->setText(p.model);
        ui.dsbLlmTemperature->setValue(p.temperature);
    }

    updatingProviderFields_ = false;
}

void Settings::loadNetworkSearchProviders()
{
    updatingNetworkSearchProviderFields_ = true;

    auto *setting = windowManager_->setting();
    const bool enabled = setting->webSearchEnabled();
    cbWebSearchEnabled_->setChecked(enabled);
    cbNetworkSearchProviders_->setEnabled(enabled);
    leNetworkSearchApiKey_->setEnabled(enabled);
    const QString provider = setting->webSearchProvider();
    const int providerIndex = cbNetworkSearchProviders_->findData(provider);
    cbNetworkSearchProviders_->setCurrentIndex(providerIndex >= 0 ? providerIndex : 0);
    leNetworkSearchApiKey_->setText(setting->webSearchApiKey());

    updatingNetworkSearchProviderFields_ = false;
}

void Settings::onLlmProviderSelected(int index)
{
    if (updatingProviderFields_) return;

    auto *setting    = windowManager_->setting();
    const auto providers = setting->llmProviders();
    if (index < 0 || index >= providers.size()) return;

    setting->setLlmActiveProviderIndex(index);

    updatingProviderFields_ = true;
    const LlmProviderConfig &p = providers.at(index);
    leLlmProviderName_->setText(p.name);
    ui.leLlmApiBaseUrl->setText(p.apiBaseUrl);
    ui.leLlmApiKey->setText(p.apiKey);
    ui.leLlmModel->setText(p.model);
    ui.dsbLlmTemperature->setValue(p.temperature);
    updatingProviderFields_ = false;
}

void Settings::onLlmProviderFieldChanged()
{
    if (updatingProviderFields_) return;

    auto *setting  = windowManager_->setting();
    const int index    = cbLlmProviders_->currentIndex();
    auto providers = setting->llmProviders();
    if (index < 0 || index >= providers.size()) return;

    LlmProviderConfig &p = providers[index];
    p.name        = leLlmProviderName_->text().trimmed();
    p.apiBaseUrl  = ui.leLlmApiBaseUrl->text().trimmed();
    p.apiKey      = ui.leLlmApiKey->text().trimmed();
    p.model       = ui.leLlmModel->text().trimmed();
    p.temperature = ui.dsbLlmTemperature->value();
    setting->setLlmProviders(providers);

    // Keep combo box display name in sync
    updatingProviderFields_ = true;
    cbLlmProviders_->setItemText(index, p.name.isEmpty() ? QStringLiteral("未命名") : p.name);
    updatingProviderFields_ = false;
    showStatusTip(QStringLiteral("厂商配置已保存"));
}

void Settings::onAddLlmProvider()
{
    auto *setting  = windowManager_->setting();
    auto providers = setting->llmProviders();

    LlmProviderConfig newProvider;
    newProvider.name        = QStringLiteral("新厂商");
    newProvider.temperature = 0.7;
    providers.append(newProvider);

    setting->setLlmProviders(providers);
    setting->setLlmActiveProviderIndex(providers.size() - 1);
    loadLlmProviders();
    showStatusTip(QStringLiteral("已添加新厂商"));
}

void Settings::onWebSearchEnabledToggled(bool checked)
{
    if (updatingNetworkSearchProviderFields_) {
        return;
    }

    windowManager_->setting()->setWebSearchEnabled(checked);
    cbNetworkSearchProviders_->setEnabled(checked);
    leNetworkSearchApiKey_->setEnabled(checked);
    showStatusTip(checked ? QStringLiteral("已启用网络搜索") : QStringLiteral("已关闭网络搜索"));
}

void Settings::onNetworkSearchProviderSelected(int index)
{
    if (updatingNetworkSearchProviderFields_) {
        return;
    }

    if (index < 0) {
        return;
    }

    windowManager_->setting()->setWebSearchProvider(
        cbNetworkSearchProviders_->itemData(index).toString());
}

void Settings::onNetworkSearchProviderFieldChanged()
{
    if (updatingNetworkSearchProviderFields_) {
        return;
    }

    windowManager_->setting()->setWebSearchApiKey(
        leNetworkSearchApiKey_->text().trimmed());
}

void Settings::onDelLlmProvider()
{
    auto *setting  = windowManager_->setting();
    auto providers = setting->llmProviders();
    if (providers.size() <= 1) {
        showStatusTip(QStringLiteral("至少保留一个厂商"), false);
        return;
    }

    const int index = cbLlmProviders_->currentIndex();
    if (index < 0 || index >= providers.size()) return;

    providers.removeAt(index);
    setting->setLlmProviders(providers);
    setting->setLlmActiveProviderIndex(qMin(index, providers.size() - 1));
    loadLlmProviders();
    showStatusTip(QStringLiteral("已删除厂商"));
}

void Settings::loadTextSelectionActions(int preferredRow)
{
    if (!listTextSelectionActions_) {
        return;
    }

    const QList<TextSelectionActionConfig> actions =
        windowManager_->setting()->textSelectionActions();
    const int previousRow = listTextSelectionActions_->currentRow();

    {
        const QSignalBlocker blocker(listTextSelectionActions_);
        listTextSelectionActions_->clear();
        for (const TextSelectionActionConfig &action : actions) {
            auto *item = new QListWidgetItem(
                action.label.trimmed().isEmpty() ? QStringLiteral("未命名") : action.label.trimmed());
            item->setData(Qt::UserRole, action.id);
            listTextSelectionActions_->addItem(item);
        }
    }

    if (actions.isEmpty()) {
        onTextSelectionActionSelected(-1);
        return;
    }

    int targetRow = preferredRow;
    if (targetRow < 0) {
        targetRow = qMin(previousRow, actions.size() - 1);
    }
    if (targetRow < 0) {
        targetRow = 0;
    }

    listTextSelectionActions_->setCurrentRow(targetRow);
    onTextSelectionActionSelected(targetRow);
}

void Settings::onImageTokenSavingToggled(bool checked)
{
    windowManager_->setting()->setLlmImageTokenSavingEnabled(checked);
    showStatusTip(checked ? QStringLiteral("已开启图片省token") : QStringLiteral("已关闭图片省token"));
}

void Settings::onTextSelectionEnabledToggled(bool checked)
{
    windowManager_->setting()->setTextSelectionEnabled(checked);
    emit windowManager_->sigSettingChanged();
    showStatusTip(checked ? QStringLiteral("已启用划词功能") : QStringLiteral("已关闭划词功能"));
}

void Settings::onTextSelectionActionSelected(int row)
{
    updatingTextSelectionActionFields_ = true;

    const QList<TextSelectionActionConfig> actions =
        windowManager_->setting()->textSelectionActions();
    const bool valid = row >= 0 && row < actions.size();

    if (leTextSelectionActionLabel_) {
        leTextSelectionActionLabel_->setEnabled(valid);
        leTextSelectionActionLabel_->setText(valid ? actions.at(row).label : QString());
    }
    if (teTextSelectionActionPrompt_) {
        teTextSelectionActionPrompt_->setEnabled(valid);
        teTextSelectionActionPrompt_->setPlainText(valid ? actions.at(row).prompt : QString());
    }
    if (btnDelTextSelectionAction_) {
        btnDelTextSelectionAction_->setEnabled(valid && actions.size() > 1);
    }

    updatingTextSelectionActionFields_ = false;
}

void Settings::onTextSelectionActionFieldChanged()
{
    if (updatingTextSelectionActionFields_ || !listTextSelectionActions_) {
        return;
    }

    const int row = listTextSelectionActions_->currentRow();
    QList<TextSelectionActionConfig> actions =
        windowManager_->setting()->textSelectionActions();
    if (row < 0 || row >= actions.size()) {
        return;
    }

    TextSelectionActionConfig &action = actions[row];
    action.label = leTextSelectionActionLabel_ ? leTextSelectionActionLabel_->text().trimmed() : QString();
    action.prompt = teTextSelectionActionPrompt_ ? teTextSelectionActionPrompt_->toPlainText().trimmed() : QString();

    windowManager_->setting()->setTextSelectionActions(actions);

    if (QListWidgetItem *item = listTextSelectionActions_->item(row)) {
        item->setText(action.label.isEmpty() ? QStringLiteral("未命名") : action.label);
    }
}

void Settings::onAddTextSelectionAction()
{
    QList<TextSelectionActionConfig> actions =
        windowManager_->setting()->textSelectionActions();

    TextSelectionActionConfig action;
    action.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    actions.append(action);

    windowManager_->setting()->setTextSelectionActions(actions);
    const int newRow = actions.size() - 1;
    loadTextSelectionActions(newRow);
    showStatusTip(QStringLiteral("已新增划词动作"));
}

void Settings::onDelTextSelectionAction()
{
    if (!listTextSelectionActions_) {
        return;
    }

    QList<TextSelectionActionConfig> actions =
        windowManager_->setting()->textSelectionActions();
    if (actions.size() <= 1) {
        showStatusTip(QStringLiteral("至少保留一个划词动作"), false);
        return;
    }

    const int row = listTextSelectionActions_->currentRow();
    if (row < 0 || row >= actions.size()) {
        return;
    }

    actions.removeAt(row);
    windowManager_->setting()->setTextSelectionActions(actions);
    loadTextSelectionActions(qMin(row, actions.size() - 1));
    showStatusTip(QStringLiteral("已删除划词动作"));
}

void Settings::onGitHubFieldChanged()
{
    GitHubImageBedConfig config;
    config.owner      = ui.leGitHubOwner->text().trimmed();
    config.repo       = ui.leGitHubRepo->text().trimmed();
    config.branch     = ui.leGitHubBranch->text().trimmed();
    config.token      = ui.leGitHubToken->text().trimmed();
    config.pathPrefix = ui.leGitHubPathPrefix->text().trimmed();
    config.cdnUrl     = ui.leGitHubCdnUrl->text().trimmed();
    windowManager_->setting()->setGitHubImageBedConfig(config);
}

void Settings::onPaddleOcrChanged()
{
    PaddleOcrConfig config;
    config.enabled = ui.cbPaddleOcrEnabled->isChecked();
    config.jobUrl = ui.lePaddleOcrUrl->text().trimmed();
    config.token = ui.lePaddleOcrToken->text().trimmed();
    config.model = ui.lePaddleOcrModel->text().trimmed();
    windowManager_->setting()->setPaddleOcrConfig(config);
    ui.lePaddleOcrUrl->setEnabled(config.enabled);
    ui.lePaddleOcrToken->setEnabled(config.enabled);
    ui.lePaddleOcrModel->setEnabled(config.enabled);
}

void Settings::onChatWindowSettingChanged()
{
    SettingModel *setting = windowManager_->setting();
    setting->setNotificationWindowSize(QSize(ui.sbNotifyWidth->value(), ui.sbNotifyHeight->value()));
    setting->setTrayNotificationPosition(static_cast<TrayNotificationPosition>(ui.cbNotifyPosition->currentIndex()));
    showStatusTip(QStringLiteral("对话窗口设置已更新"));
}

void Settings::loadToolSettings()
{
    if (!listToolItems_) return;

    const QStringList disabledTools = windowManager_->setting()->llmDisabledTools();
    updatingToolCheckBoxes_ = true;
    for (int i = 0; i < listToolItems_->count(); ++i) {
        QListWidgetItem *item = listToolItems_->item(i);
        if (!item) {
            continue;
        }
        const QString toolName = item->data(Qt::UserRole).toString();
        item->setCheckState(disabledTools.contains(toolName) ? Qt::Unchecked : Qt::Checked);
    }
    updatingToolCheckBoxes_ = false;
    if (cbToolAutoPermission_) {
        const QSignalBlocker blocker(cbToolAutoPermission_);
        cbToolAutoPermission_->setChecked(windowManager_->setting()->llmToolAutoPermission());
    }
}

void Settings::onToolCheckChanged()
{
    if (updatingToolCheckBoxes_) return;

    QStringList disabledTools;
    if (!listToolItems_) {
        return;
    }
    for (int i = 0; i < listToolItems_->count(); ++i) {
        QListWidgetItem *item = listToolItems_->item(i);
        if (item && item->checkState() != Qt::Checked) {
            disabledTools.append(item->data(Qt::UserRole).toString());
        }
    }
    windowManager_->setting()->setLlmDisabledTools(disabledTools);
}

void Settings::onToolAutoPermissionToggled(bool checked)
{
    windowManager_->setting()->setLlmToolAutoPermission(checked);
}
