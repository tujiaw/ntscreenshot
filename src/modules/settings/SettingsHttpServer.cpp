#include "Settings.h"

#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QFutureWatcher>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStyle>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include "app/WindowManager.h"
#include "core/platform/Util.h"
#include "core/settings/SettingModel.h"
#include "core/theme/UiStyler.h"

namespace {
void setStyleProperty(QWidget* widget, const char* name, const QString& value)
{
    if (!widget || widget->property(name).toString() == value) return;
    widget->setProperty(name, value);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

// A bind failure reaches us as a whole python traceback. The status line has room
// for one readable sentence, so the frame lines are dropped and the actual
// exception (the last one left) is what gets shown; the caller keeps the full text
// for the dialog's details.
QString summarizeHttpServerError(const QString& message)
{
    const QStringList lines = message.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    QString summary;
    for (const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()
            || trimmed.startsWith(QStringLiteral("Traceback"))
            || trimmed.startsWith(QStringLiteral("File \""))
            || trimmed.startsWith(QLatin1Char('^'))) {
            continue;
        }
        summary = trimmed;
    }
    if (summary.isEmpty()) {
        summary = message.simplified();
    }
    if (summary.size() > 120) {
        summary = summary.left(117) + QStringLiteral("...");
    }
    return summary;
}

}

void Settings::initHttpServerTab()
{
    auto* page = new QWidget(this);
    page->setObjectName(QStringLiteral("httpServerPage"));
    auto* layout = new QVBoxLayout(page);
    Util::scaleLayoutMargins(layout, 14, 12, 14, 14);
    layout->setSpacing(Util::scaleSize(12));

    auto* hint = new QLabel(
        QCoreApplication::translate("App", "启动后，局域网内设备可访问此目录；关闭设置窗口不会停止服务。"),
        page);
    hint->setObjectName(QStringLiteral("httpServerHint"));
    hint->setWordWrap(true);
    layout->addWidget(hint);

    // This page drives `python -m http.server`, so without an interpreter there is
    // nothing to start. Saying so up front beats a failure on the first click.
    labelHttpPythonNotice_ = new QLabel(page);
    labelHttpPythonNotice_->setObjectName(QStringLiteral("httpServerPythonNotice"));
    labelHttpPythonNotice_->setWordWrap(true);
    labelHttpPythonNotice_->setVisible(false);
    layout->addWidget(labelHttpPythonNotice_);

    auto* configPanel = new QFrame(page);
    configPanel->setObjectName(QStringLiteral("httpServerConfigPanel"));
    configPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* configLayout = new QVBoxLayout(configPanel);
    Util::scaleLayoutMargins(configLayout, 16, 14, 16, 16);
    configLayout->setSpacing(Util::scaleSize(12));

    auto* configTitle = new QLabel(QCoreApplication::translate("App", "服务配置"), configPanel);
    configTitle->setObjectName(QStringLiteral("httpServerSectionTitle"));
    configLayout->addWidget(configTitle);

    auto makeFieldLabel = [configPanel](const QString& text) {
        auto* label = new QLabel(text, configPanel);
        label->setObjectName(QStringLiteral("httpServerFieldLabel"));
        return label;
    };

    // -d/--directory
    configLayout->addWidget(makeFieldLabel(QCoreApplication::translate("App", "共享目录")));
    auto* dirRow = new QHBoxLayout();
    dirRow->setSpacing(Util::scaleSize(8));
    leHttpDirectory_ = new QLineEdit(configPanel);
    leHttpDirectory_->setReadOnly(true);
    leHttpDirectory_->setPlaceholderText(QCoreApplication::translate("App", "选择要共享的目录"));
    leHttpDirectory_->setMinimumHeight(Util::scaleSize(34));
    btnHttpBrowse_ = new QPushButton(QCoreApplication::translate("App", "选择目录"), configPanel);
    btnHttpBrowse_->setMinimumHeight(Util::scaleSize(34));
    btnHttpBrowse_->setFixedWidth(Util::scaleSize(88));
    UiStyler::setRole(btnHttpBrowse_, UiRole::SecondaryButton);
    dirRow->addWidget(leHttpDirectory_, 1);
    dirRow->addWidget(btnHttpBrowse_);
    configLayout->addLayout(dirRow);

    // 网络参数使用三列结构，标签和输入控件形成稳定的垂直对齐。
    auto* parameterGrid = new QGridLayout();
    parameterGrid->setContentsMargins(0, 0, 0, 0);
    parameterGrid->setHorizontalSpacing(Util::scaleSize(10));
    parameterGrid->setVerticalSpacing(Util::scaleSize(6));
    parameterGrid->addWidget(makeFieldLabel(QCoreApplication::translate("App", "绑定地址")), 0, 0);
    parameterGrid->addWidget(makeFieldLabel(QCoreApplication::translate("App", "端口")), 0, 1);
    parameterGrid->addWidget(makeFieldLabel(QCoreApplication::translate("App", "协议")), 0, 2);

    leHttpBind_ = new QLineEdit(configPanel);
    leHttpBind_->setPlaceholderText(QStringLiteral("0.0.0.0 / 127.0.0.1 / ::"));
    leHttpBind_->setToolTip(
        QCoreApplication::translate("App", "python http.server 的 --bind；0.0.0.0 表示局域网内可访问"));
    leHttpBind_->setMinimumHeight(Util::scaleSize(34));
    sbHttpPort_ = new QSpinBox(configPanel);
    sbHttpPort_->setRange(1, 65535);
    sbHttpPort_->setValue(8000);
    sbHttpPort_->setToolTip(QCoreApplication::translate("App", "python http.server 的 --port"));
    sbHttpPort_->setMinimumHeight(Util::scaleSize(34));
    cbHttpProtocol_ = new QComboBox(configPanel);
    cbHttpProtocol_->addItem(QStringLiteral("HTTP/1.1"), QStringLiteral("HTTP/1.1"));
    cbHttpProtocol_->addItem(QStringLiteral("HTTP/1.0"), QStringLiteral("HTTP/1.0"));
    cbHttpProtocol_->setToolTip(QCoreApplication::translate("App", "python http.server 的 --protocol"));
    cbHttpProtocol_->setMinimumHeight(Util::scaleSize(34));
    parameterGrid->addWidget(leHttpBind_, 1, 0);
    parameterGrid->addWidget(sbHttpPort_, 1, 1);
    parameterGrid->addWidget(cbHttpProtocol_, 1, 2);
    parameterGrid->setColumnStretch(0, 3);
    parameterGrid->setColumnStretch(1, 1);
    parameterGrid->setColumnStretch(2, 1);
    configLayout->addLayout(parameterGrid);

    // --cgi
    auto* cgiRow = new QHBoxLayout();
    cgiRow->setContentsMargins(0, Util::scaleSize(2), 0, 0);
    cgiRow->setSpacing(Util::scaleSize(8));
    cbHttpCgi_ = new QCheckBox(QCoreApplication::translate("App", "启用 CGI 脚本"), configPanel);
    auto* cgiHint = new QLabel(QCoreApplication::translate("App", "仅在信任共享目录内容时启用"), configPanel);
    cgiHint->setObjectName(QStringLiteral("httpServerFieldHelp"));
    cgiRow->addWidget(cbHttpCgi_);
    cgiRow->addWidget(cgiHint);
    cgiRow->addStretch();
    configLayout->addLayout(cgiRow);

    // Browsers refuse a fixed set of ports outright, so binding one of them
    // produces a server that works but that no browser can open. Warning here,
    // while the port is still being chosen, is what turns that into an
    // explanation instead of a blank page.
    labelHttpPortWarning_ = new QLabel(configPanel);
    labelHttpPortWarning_->setObjectName(QStringLiteral("httpServerPortWarning"));
    labelHttpPortWarning_->setWordWrap(true);
    labelHttpPortWarning_->setVisible(false);
    configLayout->addWidget(labelHttpPortWarning_);
    configPanel->setMinimumHeight(configLayout->sizeHint().height());
    layout->addWidget(configPanel);

    auto* statusPanel = new QFrame(page);
    statusPanel->setObjectName(QStringLiteral("httpServerStatusPanel"));
    auto* statusLayout = new QVBoxLayout(statusPanel);
    Util::scaleLayoutMargins(statusLayout, 16, 12, 16, 12);
    statusLayout->setSpacing(Util::scaleSize(8));

    auto* statusRow = new QHBoxLayout();
    statusRow->setSpacing(Util::scaleSize(8));
    labelHttpStatusDot_ = new QLabel(statusPanel);
    labelHttpStatusDot_->setObjectName(QStringLiteral("httpServerStatusDot"));
    labelHttpStatusDot_->setFixedSize(Util::scaleSize(8), Util::scaleSize(8));
    labelHttpStatus_ = new QLabel(QCoreApplication::translate("App", "服务已停止"), statusPanel);
    labelHttpStatus_->setObjectName(QStringLiteral("httpServerStatusText"));
    btnHttpOpen_ = new QPushButton(QCoreApplication::translate("App", "在浏览器中打开"), statusPanel);
    btnHttpOpen_->setObjectName(QStringLiteral("httpServerOpenButton"));
    btnHttpStart_ = new QPushButton(QCoreApplication::translate("App", "启动服务"), statusPanel);
    btnHttpStop_ = new QPushButton(QCoreApplication::translate("App", "停止"), statusPanel);
    for (QPushButton* button : {btnHttpOpen_, btnHttpStart_, btnHttpStop_}) {
        button->setMinimumHeight(Util::scaleSize(34));
    }
    btnHttpOpen_->setMinimumWidth(Util::scaleSize(116));
    btnHttpStart_->setMinimumWidth(Util::scaleSize(88));
    btnHttpStop_->setMinimumWidth(Util::scaleSize(68));
    UiStyler::setRole(btnHttpOpen_, UiRole::GhostButton);
    UiStyler::setRole(btnHttpStart_, UiRole::PrimaryButton);
    UiStyler::setRole(btnHttpStop_, UiRole::DangerButton);
    statusRow->addWidget(labelHttpStatusDot_, 0, Qt::AlignVCenter);
    statusRow->addWidget(labelHttpStatus_);
    statusRow->addStretch();
    statusRow->addWidget(btnHttpOpen_);
    statusRow->addWidget(btnHttpStop_);
    statusRow->addWidget(btnHttpStart_);
    statusLayout->addLayout(statusRow);

    teHttpAddress_ = new QPlainTextEdit(statusPanel);
    teHttpAddress_->setObjectName(QStringLiteral("httpServerAddress"));
    teHttpAddress_->setReadOnly(true);
    teHttpAddress_->setFrameStyle(QFrame::NoFrame);
    teHttpAddress_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    teHttpAddress_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    teHttpAddress_->setFixedHeight(Util::scaleSize(68));
    teHttpAddress_->setPlainText(QCoreApplication::translate("App", "启动后将在这里显示访问地址"));
    statusLayout->addWidget(teHttpAddress_);
    layout->addWidget(statusPanel);
    layout->addStretch();

    const int aboutIndex = ui.tabWidget->indexOf(ui.tab_2);
    if (aboutIndex >= 0) {
        ui.tabWidget->insertTab(aboutIndex, page, QCoreApplication::translate("App", "HTTP 服务"));
    } else {
        ui.tabWidget->addTab(page, QCoreApplication::translate("App", "HTTP 服务"));
    }

    // 预填默认参数：一次点击「启动」即可用。首次使用且无保存目录时，
    // 默认共享系统「下载」目录，取不到再回退到用户主目录。
    HttpServerConfig cfg = windowManager_->setting()->httpServerConfig();
    if (cfg.directory.trimmed().isEmpty()) {
        const QString downloads =
            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        cfg.directory = downloads.trimmed().isEmpty() ? QDir::homePath() : downloads;
        windowManager_->setting()->setHttpServerConfig(cfg);
    }
    leHttpDirectory_->setText(QDir::toNativeSeparators(cfg.directory));
    sbHttpPort_->setValue(cfg.port);
    leHttpBind_->setText(cfg.bind);
    const int protocolIndex = cbHttpProtocol_->findData(cfg.protocol);
    cbHttpProtocol_->setCurrentIndex(protocolIndex >= 0 ? protocolIndex : 0);
    cbHttpCgi_->setChecked(cfg.cgi);

    auto* controller = windowManager_->httpServer();

    connect(sbHttpPort_, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int) { updateHttpPortWarning(); });
    updateHttpPortWarning();

    // Discovery waits for external processes; keep it off the UI thread. The
    // worker owns its controller and never captures this dialog's lifetime.
    auto* pythonWatcher = new QFutureWatcher<bool>(this);
    connect(pythonWatcher, &QFutureWatcher<bool>::finished, this, [this, pythonWatcher]() {
        httpPythonReady_ = pythonWatcher->result();
        httpPythonProbed_ = true;
        pythonWatcher->deleteLater();
        refreshHttpServerState();
    });
    pythonWatcher->setFuture(QtConcurrent::run([]() {
        HttpServerController probe;
        return probe.pythonAvailable();
    }));

    connect(btnHttpBrowse_, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getExistingDirectory(
            this, QCoreApplication::translate("App", "选择要共享的目录"), leHttpDirectory_->text());
        if (path.isEmpty()) {
            return;
        }
        leHttpDirectory_->setText(QDir::toNativeSeparators(path));
    });

    connect(btnHttpStart_, &QPushButton::clicked, this, [this]() {
        const QString dir = leHttpDirectory_->text().trimmed();
        const QFileInfo dirInfo(dir);
        if (dir.isEmpty() || !dirInfo.isDir()) {
            showStatusTip(QCoreApplication::translate("App", "请先选择一个有效的共享目录"), false);
            return;
        }

        HttpServerController::Params params;
        params.directory = dir;
        params.port = sbHttpPort_->value();
        params.bind = leHttpBind_->text().trimmed();
        if (params.bind.isEmpty()) {
            params.bind = QStringLiteral("0.0.0.0");
        }
        params.protocol = cbHttpProtocol_->currentData().toString();
        params.cgi = cbHttpCgi_->isChecked();

        // A restricted port serves fine but no browser will ever open it, so say
        // why before starting rather than after the user stares at a blank page.
        if (HttpServerController::isBrowserBlockedPort(params.port)) {
            QMessageBox box(QMessageBox::Warning,
                            QCoreApplication::translate("App", "端口 %1 无法用浏览器打开").arg(params.port),
                            QStringLiteral(
                                "端口 %1 属于浏览器受限端口（SSH 等系统服务的常用端口），"
                                "Chrome、Edge、Firefox 会直接拒绝访问并提示 ERR_UNSAFE_PORT。\n\n"
                                "服务本身可以正常启动，但它只能用 curl 等工具访问。"
                                "如需在浏览器中打开，请改用 8000、8080 等端口。\n\n"
                                "仍要在此端口启动吗？")
                                .arg(params.port),
                            QMessageBox::NoButton, this);
            QPushButton* proceed =
                box.addButton(QCoreApplication::translate("App", "仍然启动"), QMessageBox::AcceptRole);
            box.addButton(QCoreApplication::translate("App", "取消"), QMessageBox::RejectRole);
            box.setDefaultButton(proceed);
            box.exec();
            if (box.clickedButton() != proceed) {
                return;
            }
        }

        HttpServerConfig cfg;
        cfg.directory = params.directory;
        cfg.port = params.port;
        cfg.bind = params.bind;
        cfg.protocol = params.protocol;
        cfg.cgi = params.cgi;
        windowManager_->setting()->setHttpServerConfig(cfg);
        emit windowManager_->sigSettingChanged();

        auto* server = windowManager_->httpServer();
        // start() waits for the port to actually accept connections, so this can
        // block for a moment; showing the intermediate state keeps the window from
        // looking frozen while it does.
        labelHttpStatus_->setText(QCoreApplication::translate("App", "正在启动…"));
        setStyleProperty(labelHttpStatusDot_, "status", QStringLiteral("starting"));
        setStyleProperty(labelHttpStatus_, "status", QStringLiteral("starting"));
        labelHttpStatus_->repaint();

        if (server->start(params)) {
            showStatusTip(QCoreApplication::translate("App", "HTTP 服务已启动"));
            return;
        }

        const QString reason = server->lastError();
        refreshHttpServerState();
        showHttpServerError(QCoreApplication::translate("App", "无法启动 HTTP 服务"), reason);
    });

    connect(btnHttpStop_, &QPushButton::clicked, this, [this]() {
        windowManager_->httpServer()->stop();
        showStatusTip(QCoreApplication::translate("App", "HTTP 服务已停止"));
    });

    connect(btnHttpOpen_, &QPushButton::clicked, this, [this]() {
        if (!httpPrimaryUrl_.isEmpty()) {
            Util::shellExecute(httpPrimaryUrl_);
        }
    });

    connect(controller, &HttpServerController::runningChanged, this,
            [this](bool) { refreshHttpServerState(); });
    connect(controller, &HttpServerController::errorOccurred, this,
            [this](const QString& message) {
        // Surfaces failures that happen after a successful start (the server
        // dying later). Failures during start() come back through lastError() and
        // are reported by the click handler instead, so they are not shown twice.
        refreshHttpServerState();
        showHttpServerError(QCoreApplication::translate("App", "HTTP 服务已停止"), message);
    });

    refreshHttpServerState();
}

void Settings::updateHttpPortWarning()
{
    if (!labelHttpPortWarning_ || !sbHttpPort_) {
        return;
    }
    const int port = sbHttpPort_->value();
    const bool blocked = HttpServerController::isBrowserBlockedPort(port);
    labelHttpPortWarning_->setVisible(blocked);
    if (blocked) {
        labelHttpPortWarning_->setText(
            QCoreApplication::translate("App",
                "端口 %1 是浏览器受限端口（SSH 等系统服务的常用端口），"
                "Chrome/Edge/Firefox 会拒绝访问并提示 ERR_UNSAFE_PORT。"
                "服务可以启动，但只有 curl 等工具能访问；建议改用 8000、8080 等端口。")
                .arg(port));
    }
}

void Settings::showHttpServerError(const QString& title, const QString& detail)
{
    const QString summary =
        detail.trimmed().isEmpty() ? QCoreApplication::translate("App", "HTTP 服务启动失败。") : summarizeHttpServerError(detail);
    if (labelHttpStatus_) {
        labelHttpStatus_->setText(summary);
        setStyleProperty(labelHttpStatusDot_, "status", QStringLiteral("error"));
        setStyleProperty(labelHttpStatus_, "status", QStringLiteral("error"));
    }
    showStatusTip(summary, false);

    QMessageBox box(QMessageBox::Warning, title, summary, QMessageBox::Ok, this);
    if (summary != detail.trimmed()) {
        // The traceback is what actually explains the failure, so it stays one
        // click away instead of being pasted into the status line.
        box.setDetailedText(detail);
    }
    box.exec();
}

void Settings::refreshHttpServerState()
{
    auto* controller = windowManager_->httpServer();
    if (!controller) {
        return;
    }

    const bool running = controller->isRunning();
    // Keep Start disabled until discovery finishes, avoiding a synchronous
    // cache initialization wait on the UI thread.
    const bool pythonMissing = httpPythonProbed_ && !httpPythonReady_;
    if (labelHttpPythonNotice_) {
        labelHttpPythonNotice_->setVisible(pythonMissing);
        if (pythonMissing) {
            labelHttpPythonNotice_->setText(QStringLiteral(
                "此功能需要安装 Python 3：未检测到可用的 Python 解释器，HTTP 服务无法启动。"
                "请从 https://www.python.org/downloads/ 安装，安装时勾选 “Add python.exe to PATH”，"
                "完成后重新打开本程序。"));
        }
    }

    leHttpDirectory_->setEnabled(!running);
    btnHttpBrowse_->setEnabled(!running);
    sbHttpPort_->setEnabled(!running);
    leHttpBind_->setEnabled(!running);
    cbHttpProtocol_->setEnabled(!running);
    cbHttpCgi_->setEnabled(!running);
    btnHttpStart_->setEnabled(!running && httpPythonProbed_ && !pythonMissing);
    btnHttpStop_->setEnabled(running);
    btnHttpOpen_->setEnabled(running);

    if (!running) {
        labelHttpStatus_->setText(QCoreApplication::translate("App", "服务已停止"));
        setStyleProperty(labelHttpStatusDot_, "status", QStringLiteral("stopped"));
        setStyleProperty(labelHttpStatus_, "status", QStringLiteral("stopped"));
        teHttpAddress_->setPlainText(QCoreApplication::translate("App", "启动后将在这里显示访问地址"));
        httpPrimaryUrl_.clear();
        return;
    }

    labelHttpStatus_->setText(QCoreApplication::translate("App", "服务运行中"));
    setStyleProperty(labelHttpStatusDot_, "status", QStringLiteral("running"));
    setStyleProperty(labelHttpStatus_, "status", QStringLiteral("running"));
    const int port = controller->port();
    QString bind = controller->bind().trimmed();
    if (bind.isEmpty()) {
        bind = QStringLiteral("0.0.0.0");
    }
    QStringList urls;
    const bool serveAll = bind == QStringLiteral("0.0.0.0") || bind == QStringLiteral("::");
    if (serveAll) {
        urls << QStringLiteral("http://127.0.0.1:%1/").arg(port);
        const QStringList ips = controller->localIpv4Addresses();
        for (const QString& ip : ips) {
            urls << QStringLiteral("http://%1:%2/").arg(ip).arg(port);
        }
    } else {
        urls << QStringLiteral("http://%1:%2/").arg(bind).arg(port);
    }
    teHttpAddress_->setPlainText(urls.join(QLatin1Char('\n')));
    httpPrimaryUrl_ = urls.constFirst();
}
