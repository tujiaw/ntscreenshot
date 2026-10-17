#include "FavoriteSyncPage.h"
#include "FavoriteSyncController.h"
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace { QString T(const char* source) { return QCoreApplication::translate("App", source); } }

FavoriteSyncPage::FavoriteSyncPage(FavoriteSyncController* controller, QWidget* parent)
    : QWidget(parent), controller_(controller) {
    setObjectName(QStringLiteral("favoriteSyncPage"));
    setMinimumWidth(0);
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    scrollArea_ = new QScrollArea(this);
    scrollArea_->setFrameShape(QFrame::NoFrame);
    scrollArea_->setWidgetResizable(true);
    scrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* content = new QWidget(scrollArea_);
    content->setObjectName(QStringLiteral("favoriteSyncContent"));
    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(16, 16, 20, 20);
    root->setSpacing(12);
    root->setSizeConstraint(QLayout::SetMinimumSize);
    scrollArea_->setWidget(content);
    outer->addWidget(scrollArea_, 1);
    auto* title = new QLabel(T("让收藏跟随你的 GitHub 账号"), this);
    title->setWordWrap(true);
    QFont titleFont = title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() + 3);
    titleFont.setBold(true);
    title->setFont(titleFont);
    root->addWidget(title);
    auto* description = new QLabel(T("在其他设备登录同一账号，即可同步收藏的文字和图片。普通剪切板历史仅保存在本机。"), this);
    description->setWordWrap(true);
    root->addWidget(description);
    account_ = new QLabel(this);
    account_->setTextFormat(Qt::PlainText);
    account_->setWordWrap(true);
    account_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    root->addWidget(account_);
    status_ = new QLabel(this);
    status_->setObjectName(QStringLiteral("favoriteSyncStatus"));
    status_->setWordWrap(true);
    status_->setTextFormat(Qt::PlainText);
    status_->setAccessibleName(T("同步状态"));
    root->addWidget(status_);
    details_ = new QLabel(this);
    details_->setWordWrap(true);
    root->addWidget(details_);

    auto* privacy = new QLabel(T("GitHub 仅用于识别账号，不申请仓库权限。登录后合并本机与云端收藏，并自动同步更改。"), this);
    privacy->setWordWrap(true);
    auto* limits = new QLabel(T("同步支持单条文字 100 KB、单张图片 2 MB。超出限制的收藏仍保存在本机。"), this);
    limits->setWordWrap(true);

    auto* actions = new QVBoxLayout();
    signIn_ = new QPushButton(T("使用 GitHub 登录"), this);
    signIn_->setObjectName(QStringLiteral("favoriteSyncSignIn"));
    signIn_->setProperty("uiRole", "primary");
    sync_ = new QPushButton(T("立即同步"), this);
    sync_->setObjectName(QStringLiteral("favoriteSyncNow"));
    sync_->setProperty("uiRole", "primary");
    signOut_ = new QPushButton(T("退出登录"), this);
    actions->addWidget(signIn_);
    actions->addWidget(sync_);
    actions->addWidget(signOut_);
    root->addLayout(actions);

    verification_ = new QGroupBox(T("完成 GitHub 授权"), this);
    auto* verifyLayout = new QVBoxLayout(verification_);
    verifyLayout->setContentsMargins(12, 24, 12, 12);
    verifyLayout->setSpacing(10);
    verification_->setMinimumHeight(200);
    auto* instructions = new QLabel(T("复制下方验证码，在 GitHub 页面输入并授权。此窗口会自动等待登录结果。"), verification_);
    instructions->setWordWrap(true);
    verifyLayout->addWidget(instructions);
    code_ = new QLabel(verification_);
    code_->setObjectName(QStringLiteral("favoriteSyncUserCode"));
    code_->setAlignment(Qt::AlignCenter);
    code_->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    QFont codeFont = code_->font();
    codeFont.setPointSizeF(codeFont.pointSizeF() + 8);
    codeFont.setBold(true);
    code_->setFont(codeFont);
    code_->setStyleSheet(QStringLiteral("font-size: 24px; font-weight: 600; padding: 6px;"));
    code_->setMinimumHeight(44);
    verifyLayout->addWidget(code_);
    auto* verifyActions = new QVBoxLayout();
    copyCode_ = new QPushButton(T("复制验证码"), verification_);
    openBrowser_ = new QPushButton(T("复制验证码并打开 GitHub"), verification_);
    openBrowser_->setProperty("uiRole", "primary");
    verifyActions->addWidget(copyCode_);
    verifyActions->addWidget(openBrowser_);
    verifyLayout->addLayout(verifyActions);
    root->addWidget(verification_);
    root->addWidget(privacy);
    root->addWidget(limits);

    root->addStretch();
    connect(controller_, &FavoriteSyncController::statusChanged, this, &FavoriteSyncPage::Refresh);
    connect(signIn_, &QPushButton::clicked, this, [this]() {
        if (controller_->SigningIn()) controller_->CancelSignIn();
        else controller_->SignIn();
    });
    connect(sync_, &QPushButton::clicked, controller_, &FavoriteSyncController::SyncNow);
    connect(signOut_, &QPushButton::clicked, this, [this]() {
        const auto answer = QMessageBox::question(this, T("退出收藏同步"),
            T("退出后将停止云同步，本机和云端收藏都会保留。是否退出？"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer == QMessageBox::Yes) controller_->SignOut();
    });
    connect(copyCode_, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(controller_->UserCode());
        copyCode_->setText(T("已复制"));
    });
    connect(openBrowser_, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(controller_->UserCode());
        copyCode_->setText(T("已复制"));
        if (!QDesktopServices::openUrl(QUrl(controller_->VerificationUrl()))) {
            status_->setText(T("无法打开浏览器，请手动访问 https://github.com/login/device"));
        }
    });
    Refresh();
}

void FavoriteSyncPage::Refresh() {
    const bool signedIn = controller_->SignedIn();
    const bool signingIn = controller_->SigningIn();
    account_->setVisible(!controller_->Account().isEmpty());
    account_->setText(signedIn ? T("已登录 @%1").arg(controller_->Account())
                             : T("本机收藏关联账号：@%1").arg(controller_->Account()));
    status_->setText(controller_->Status());
    QString details;
    if (controller_->PendingCount()) details = T("%1 项更改等待同步").arg(controller_->PendingCount());
    if (controller_->LastSync().toMSecsSinceEpoch() > 0) {
        if (!details.isEmpty()) details += QStringLiteral(" · ");
        details += T("上次同步：%1").arg(controller_->LastSync().toLocalTime().toString(QStringLiteral("MM-dd HH:mm")));
    }
    details_->setText(details);
    details_->setVisible(!details.isEmpty());
    signIn_->setVisible(!signedIn);
    signIn_->setText(signingIn ? T("取消登录") : T("使用 GitHub 登录"));
    signIn_->setEnabled(signingIn || !controller_->Endpoint().isEmpty());
    sync_->setVisible(signedIn);
    sync_->setEnabled(!controller_->Busy());
    sync_->setText(controller_->Busy() ? T("正在同步…") : T("立即同步"));
    signOut_->setVisible(signedIn);
    setFocusProxy(signedIn ? sync_ : signIn_);
    verification_->setVisible(signingIn && !controller_->UserCode().isEmpty());
    if (code_->text() != controller_->UserCode()) copyCode_->setText(T("复制验证码"));
    code_->setText(controller_->UserCode());
    if (verification_->isVisible()) {
        verification_->layout()->activate();
        verification_->setMinimumHeight(qMax(200, verification_->layout()->heightForWidth(qMax(180, scrollArea_->viewport()->width() - 36)) + fontMetrics().height()));
    }
    const int mode = signedIn ? 2 : signingIn && !controller_->UserCode().isEmpty() ? 1 : 0;
    if (mode != displayMode_) {
        displayMode_ = mode;
        QTimer::singleShot(0, this, [this]() {
            auto* content = scrollArea_->widget();
            content->layout()->activate();
            if (!controller_->UserCode().isEmpty()) scrollArea_->ensureWidgetVisible(verification_, 0, 12);
            else scrollArea_->verticalScrollBar()->setValue(0);
        });
    }
}
