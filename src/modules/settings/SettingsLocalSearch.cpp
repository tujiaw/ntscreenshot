#include "Settings.h"
#include "SettingsInternal.h"

#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>

#include "app/WindowManager.h"
#include "core/platform/Util.h"
#include "core/settings/SettingModel.h"
#include "core/theme/ThemeManager.h"
#include "shared/foundation/IgnorePatternMatcher.h"

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

    auto* shortcutLabel = new QLabel(QCoreApplication::translate("App", "快捷键"), toolbar);
    shortcutLabel->setToolTip(QCoreApplication::translate("App", "用于快速显示本地搜索窗口"));
    kseLocalSearch_ = new QKeySequenceEdit(toolbar);
    kseLocalSearch_->setFixedWidth(Util::scaleSize(170));
    kseLocalSearch_->setToolTip(
        QCoreApplication::translate("App", "默认 Alt+Space；快捷键冲突时可改为 Ctrl+Alt+Space"));
    auto* presetAltSpace = new QPushButton(QStringLiteral("Alt+Space"), toolbar);
    presetAltSpace->setToolTip(QCoreApplication::translate("App", "使用默认快捷键 Alt+Space"));

    auto* separator = new QFrame(toolbar);
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);

    auto* bookmarksLabel = new QLabel(QCoreApplication::translate("App", "书签"), toolbar);
    bookmarksLabel->setToolTip(QCoreApplication::translate("App", "启用后会将浏览器书签写入本地搜索索引"));
    cbLocalSearchChromeBookmarks_ = new QCheckBox(QStringLiteral("Chrome"), toolbar);
    cbLocalSearchEdgeBookmarks_ = new QCheckBox(QStringLiteral("Edge"), toolbar);
    cbLocalSearchChromeBookmarks_->setToolTip(QCoreApplication::translate("App", "索引 Chrome 书签"));
    cbLocalSearchEdgeBookmarks_->setToolTip(QCoreApplication::translate("App", "索引 Edge 书签"));

    auto* engineLabel = new QLabel(QCoreApplication::translate("App", "搜索引擎"), toolbar);
    cbLocalSearchWebEngine_ = new QComboBox(toolbar);
    cbLocalSearchWebEngine_->setFixedWidth(Util::scaleSize(270));
    cbLocalSearchWebEngine_->setToolTip(QCoreApplication::translate("App", "输入 < 前缀进行网页搜索时使用的搜索引擎"));
    const auto engines = SettingModel::localSearchWebEngines();
    for (const auto& engine : engines) {
        const QIcon icon = engine.iconPath.isEmpty() ? QIcon() : QIcon(engine.iconPath);
        cbLocalSearchWebEngine_->addItem(icon, engine.name, engine.id);
    }

    cbLocalSearchPinyin_ = new QCheckBox(QCoreApplication::translate("App", "拼音搜索"), toolbar);
    cbLocalSearchPinyin_->setToolTip(QCoreApplication::translate("App", "支持输入中文拼音检索文件/目录/应用"));

    auto* rebuild = new QPushButton(QCoreApplication::translate("App", "重建索引"), toolbar);
    rebuild->setToolTip(QCoreApplication::translate("App", "重新扫描目录、应用和已启用的浏览器书签"));

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
        QCoreApplication::translate("App", "仅添加经常需要搜索的位置；目录变化会在后台自动同步。"), rootsTab);
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
    auto* addRoot = new QPushButton(QCoreApplication::translate("App", "添加目录"), rootsTab);
    auto* removeRoot = new QPushButton(QCoreApplication::translate("App", "删除选中"), rootsTab);
    auto* restoreRoots = new QPushButton(QCoreApplication::translate("App", "恢复默认"), rootsTab);
    restoreRoots->setToolTip(QCoreApplication::translate("App", "恢复桌面、文档、下载、图片、音乐、视频和 OneDrive"));
    rootButtons->addWidget(addRoot);
    rootButtons->addWidget(removeRoot);
    rootButtons->addStretch();
    rootButtons->addWidget(restoreRoots);
    rootsRow->addLayout(rootButtons);
    rootsLayout->addLayout(rootsRow, 1);
    listTabs->addTab(rootsTab, QCoreApplication::translate("App", "索引目录"));

    auto* excludesTab = new QWidget();
    auto* excludesLayout = new QVBoxLayout(excludesTab);
    Util::scaleLayoutMargins(excludesLayout, 12, 10, 12, 12);
    excludesLayout->setSpacing(Util::scaleSize(8));
    auto* excludesHint = new QLabel(
        QCoreApplication::translate("App", "每行一条规则，按索引根目录的相对路径匹配。"),
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
    auto* saveRules = new QPushButton(QCoreApplication::translate("App", "保存"), excludesTab);
    auto* resetRules = new QPushButton(QCoreApplication::translate("App", "恢复默认"), excludesTab);
    resetRules->setToolTip(QCoreApplication::translate("App", "恢复后点击\"保存\"以应用"));
    rulesButtons->addWidget(saveRules);
    rulesButtons->addStretch();
    rulesButtons->addWidget(resetRules);
    excludesRow->addLayout(rulesButtons);
    excludesLayout->addLayout(excludesRow, 1);
    listTabs->addTab(excludesTab, QCoreApplication::translate("App", "排除规则"));

    layout->addWidget(listTabs, 1);

    const int aboutIndex = ui.tabWidget->indexOf(ui.tab_2);
    if (aboutIndex >= 0) {
        ui.tabWidget->insertTab(aboutIndex, page, QCoreApplication::translate("App", "本地搜索"));
    } else {
        ui.tabWidget->addTab(page, QCoreApplication::translate("App", "本地搜索"));
    }

    const auto applyLocalSearchHotkey = [this](const QString& key) {
        if (windowManager_->setLocalSearchGlobalKey(key)) {
            windowManager_->setting()->setLocalSearchGlobalKey(key);
            emit windowManager_->sigSettingChanged();
            showStatusTip(QCoreApplication::translate("App", "本地搜索快捷键已更新"));
            return;
        }

        const QString reason = windowManager_->lastHotkeyError();
        const QString detail = reason.isEmpty()
            ? QCoreApplication::translate("App", "快捷键注册失败，可能被占用")
            : QCoreApplication::translate("App", "快捷键注册失败：%1").arg(reason);
        showStatusTip(detail, false);
        QMessageBox::warning(this, QCoreApplication::translate("App", "快捷键冲突"),
                             detail + QCoreApplication::translate("App", "\n请更换组合键。"));
        kseLocalSearch_->setKeySequence(QKeySequence::fromString(
            windowManager_->setting()->localSearchGlobalKey(), QKeySequence::NativeText));
    };
    connect(kseLocalSearch_, &QKeySequenceEdit::editingFinished, this,
            [this, applyLocalSearchHotkey]() {
        applyLocalSearchHotkey(settings_detail::keySequenceText(kseLocalSearch_->keySequence()));
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
        showStatusTip(QCoreApplication::translate("App", "书签来源已更新，索引将自动重建"));
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
        showStatusTip(QCoreApplication::translate("App", "搜索引擎已更新"));
    });
    connect(cbLocalSearchPinyin_, &QCheckBox::toggled, this, [this](bool checked) {
        windowManager_->setting()->setLocalSearchPinyinEnabled(checked);
        emit windowManager_->sigSettingChanged();
        showStatusTip(checked ? QCoreApplication::translate("App", "已启用拼音搜索")
                              : QCoreApplication::translate("App", "已关闭拼音搜索"));
    });
    connect(addRoot, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getExistingDirectory(this, QCoreApplication::translate("App", "选择索引目录"));
        if (path.isEmpty()) return;
        QStringList roots = windowManager_->setting()->localSearchRoots();
        if (!roots.contains(path, Qt::CaseInsensitive)) roots.push_back(path);
        windowManager_->setting()->setLocalSearchRoots(roots);
        loadLocalSearchSettings();
        emit windowManager_->sigSettingChanged();
        showStatusTip(QCoreApplication::translate("App", "已添加索引目录"));
    });
    connect(removeRoot, &QPushButton::clicked, this, [this]() {
        const int row = listLocalSearchRoots_->currentRow();
        if (row < 0) return;
        QStringList roots = windowManager_->setting()->localSearchRoots();
        roots.removeAt(row);
        windowManager_->setting()->setLocalSearchRoots(roots);
        loadLocalSearchSettings();
        emit windowManager_->sigSettingChanged();
        showStatusTip(QCoreApplication::translate("App", "已移除索引目录"));
    });
    connect(restoreRoots, &QPushButton::clicked, this, [this]() {
        const auto answer = QMessageBox::question(
            this, QCoreApplication::translate("App", "恢复默认"),
            QCoreApplication::translate("App", "将用默认目录替换当前索引目录，是否继续？"));
        if (answer != QMessageBox::Yes) return;
        windowManager_->setting()->setLocalSearchRoots(
            SettingModel::defaultLocalSearchRoots());
        loadLocalSearchSettings();
        emit windowManager_->sigSettingChanged();
        showStatusTip(QCoreApplication::translate("App", "已恢复推荐索引目录"));
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
                    showStatusTip(QCoreApplication::translate("App", "排除规则无效"), false);
                    QMessageBox::warning(this, QCoreApplication::translate("App", "排除规则无效"),
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
        if (savePatterns()) showStatusTip(QCoreApplication::translate("App", "排除规则已保存，索引将自动更新"));
    });
    connect(resetRules, &QPushButton::clicked, this, [this]() {
        teLocalSearchExcludes_->setPlainText(
            SettingModel::defaultLocalSearchExcludePatterns().join('\n'));
        teLocalSearchExcludes_->setFocus();
        showStatusTip(QCoreApplication::translate("App", "已恢复默认排除规则，点击保存生效"));
    });
    connect(rebuild, &QPushButton::clicked, this, [this, savePatterns]() {
        if (!savePatterns()) return;
        windowManager_->rebuildLocalSearchIndex();
        labelLocalSearchStatus_->setText(QCoreApplication::translate("App", "已请求重建…"));
        showStatusTip(QCoreApplication::translate("App", "索引重建已开始"));
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
        {QCoreApplication::translate("App", "程序"), Util::getRunDir()},
        {QCoreApplication::translate("App", "配置"), Util::getConfigDir()},
        {QCoreApplication::translate("App", "日志"), Util::getLogsDir()}
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
    auto* header = ui.tablePath->horizontalHeader();
    header->setStretchLastSection(false);
    ui.tablePath->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui.tablePath->setRowCount(data.size());
    ui.tablePath->setColumnCount(2);
    ui.tablePath->setTextElideMode(Qt::ElideMiddle);
    header->setSectionResizeMode(0, QHeaderView::Fixed);
    header->setSectionResizeMode(1, QHeaderView::Stretch);

    int labelWidth = 0;
    for (const auto& entry : data) {
        labelWidth = qMax(labelWidth, ui.tablePath->fontMetrics().horizontalAdvance(entry.first));
    }
    ui.tablePath->setColumnWidth(0, labelWidth + Util::scaleSize(28));

    connect(ui.tablePath, &QTableWidget::cellDoubleClicked, this, &Settings::onTablePathDoubleClicked);
    for (int row = 0; row < data.size(); row++) {
        ui.tablePath->setItem(row, 0, new QTableWidgetItem(data[row].first));
        ui.tablePath->setItem(row, 1, new QTableWidgetItem(data[row].second));
    }
}


void Settings::onTablePathDoubleClicked(int row, int col)
{
    QString text = ui.tablePath->item(row, col)->text();
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(text);
}
