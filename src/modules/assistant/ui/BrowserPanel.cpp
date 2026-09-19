#include "BrowserPanel.h"
#include "modules/assistant/runtime/tools/ToolAbort.h"
#include <QColor>
#include <QPalette>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineNewWindowRequest>
#include <QWebEnginePermission>
#include <QWebEngineWebAuthUxRequest>
#include <QAuthenticator>
#include <QInputDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QEvent>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <QThread>
#include <QApplication>
#include <QPointer>
#include <QMutex>
#include <QWaitCondition>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <QUrlQuery>
#include <atomic>
#include <utility>

namespace {
QString permissionLabel(QWebEnginePermission::PermissionType type)
{
    using P = QWebEnginePermission::PermissionType;
    switch (type) {
    case P::MediaAudioCapture: return QStringLiteral("麦克风");
    case P::MediaVideoCapture: return QStringLiteral("摄像头");
    case P::MediaAudioVideoCapture: return QStringLiteral("摄像头和麦克风");
    case P::DesktopVideoCapture: return QStringLiteral("屏幕共享");
    case P::DesktopAudioVideoCapture: return QStringLiteral("屏幕与音频共享");
    case P::MouseLock: return QStringLiteral("锁定鼠标");
    case P::Notifications: return QStringLiteral("发送通知");
    case P::Geolocation: return QStringLiteral("位置信息");
    case P::ClipboardReadWrite: return QStringLiteral("读写剪贴板");
    case P::LocalFontsAccess: return QStringLiteral("本地字体");
    default: return QStringLiteral("未知权限");
    }
}

bool browserIsDarkTheme()
{
    // 应用通过 ThemeManager 设置全局调色板，这里直接读窗口底色判断明暗，
    // 避免 BrowserPanel 依赖 core 主题模块，便于单测独立链接。
    return QApplication::palette().color(QPalette::Window).lightness() < 128;
}

constexpr int kNavIconPx = 18;
constexpr int kNavButtonPx = 28;
// 地址栏保留足够高度，避免全局输入框样式裁切文字。
constexpr int kAddressHeightPx = 32;

QString browserForegroundColor()
{
    return browserIsDarkTheme() ? QStringLiteral("#e5e7eb") : QStringLiteral("#374151");
}

QString browserNavButtonStyle()
{
    const bool dark = browserIsDarkTheme();
    const QString text = browserForegroundColor();
    const QString bg = dark ? "rgba(255,255,255,0.06)" : "rgba(15,23,42,0.05)";
    const QString hover = dark ? "rgba(255,255,255,0.12)" : "rgba(15,23,42,0.10)";
    return QStringLiteral(
        "QPushButton{border:none;border-radius:6px;background:%1;color:%2;padding:0;}"
        "QPushButton:hover{background:%3;}")
        .arg(bg, text, hover);
}

// 图标是单色透明底的 PNG，用 SourceIn 直接换成当前主题的前景色。
// BrowserPanel 不依赖 core 主题模块（便于单测独立链接），所以这里就地取色。
QIcon browserNavIcon(const QString &fileName)
{
    QPixmap pixmap(QStringLiteral(":/images/") + fileName);
    if (pixmap.isNull()) {
        return QIcon();
    }
    // 源图四周留了透明边，先裁掉再缩放，图标才能填满按钮。
    const QImage image = pixmap.toImage();
    int left = image.width(), top = image.height(), right = -1, bottom = -1;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(image.pixel(x, y)) > 8) {
                left = qMin(left, x);
                right = qMax(right, x);
                top = qMin(top, y);
                bottom = qMax(bottom, y);
            }
        }
    }
    if (right >= left && bottom >= top) {
        pixmap = pixmap.copy(left, top, right - left + 1, bottom - top + 1);
    }
    pixmap = pixmap.scaled(kNavIconPx, kNavIconPx, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QPainter painter(&pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), QColor(browserForegroundColor()));
    painter.end();
    return QIcon(pixmap);
}

// 起始页：介绍 browser use 功能的静态页面，随资源一起打包。
QUrl browserHomeUrl()
{
    return QUrl(QStringLiteral("qrc:/html/browser-home.html"));
}

// 交给用户后若一直没完成认证，避免 Agent 工作线程无限期阻塞。
// 交出控制权后的最短等待时间：用户动手需要时间，不能刚交出去就抢回来。
constexpr qint64 kManualMinWaitMs = 5000;

class BrowserPage final : public QWebEnginePage {
public:
    using QWebEnginePage::QWebEnginePage;
protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType, bool) override {
        if (url == browserHomeUrl()) return true;  // 自带起始页，其余 qrc 资源一律不放行
        return url.scheme() == "https" || url.scheme() == "http" || url == QUrl("about:blank");
    }
};

// 认证组件只用于快照提示和等待后的恢复判断，不因组件存在自动暂停普通浏览。
const char authDetection[] = R"JS(
 const ntVisible = e => !!(e.getClientRects().length && getComputedStyle(e).visibility !== 'hidden');
 const ntSensitive = e => e.matches('input[type=password],input[autocomplete=one-time-code],input[autocomplete=username]') ||
   (e.matches('input') && (/^(username|login|account)$/i.test(e.name || e.id) || !!e.form?.querySelector('input[type=password]'))) ||
   /password|passwd|otp|verification|验证码|校验码|动态码/i.test([e.name,e.id,e.autocomplete,e.placeholder,e.getAttribute('aria-label')].join(' '));
 const ntNeedsUser = () => {
   const inputs = Array.from(document.querySelectorAll('input')).filter(ntVisible);
   const pageText = document.body ? document.body.innerText : '';
   return inputs.some(ntSensitive) || /扫码登录|扫描二维码.*登录|人机验证|安全验证|verify you are human|scan.{0,30}(sign in|log in)|enter.{0,20}verification code/i.test(pageText) ||
     Array.from(document.querySelectorAll('iframe')).some(e => ntVisible(e) && /captcha|challenge/i.test(e.src));
 };
)JS";

// 轮询用：页面是否仍停在需要用户接管的认证环节。
const char authProbe[] = R"JS(
(() => {
%1
 return ntNeedsUser();
})()
)JS";

// The isolated world owns element references; websites cannot forge tool handles.
// Credential values are excluded from snapshots; only sensitive target actions require handoff.
const char script[] = R"JS(
(() => {
 const args = %2;
%1
 let state = globalThis.__ntBrowserState;
 const label = e => (e.getAttribute('aria-label') || (e.labels && Array.from(e.labels).map(l=>l.innerText).join(' ')) || e.innerText || e.placeholder || e.name || e.title || '').trim();
 const normalize = value => String(value || '').toLowerCase().replace(/\s+/g,' ').trim();
 const interactiveSelector = 'a[href],button,input,textarea,select,[role=button],[role=link],[role=checkbox],[role=tab],[contenteditable=true]';
 const interactiveElements = () => {
   const result = [], seen = new Set();
   const visit = rootNode => {
     if (!rootNode || !rootNode.querySelectorAll) return;
     rootNode.querySelectorAll(interactiveSelector).forEach(e => {
       if (!seen.has(e)) { seen.add(e); result.push(e); }
     });
     rootNode.querySelectorAll('*').forEach(e => {
       if (e.shadowRoot) visit(e.shadowRoot);
     });
   };
   visit(document);
   return result;
 };
 const resolveTarget = target => {
   const wanted = normalize(target);
   if (!wanted) return {element:null,ambiguous:false};
   const candidates = interactiveElements().filter(e => ntVisible(e) && !e.disabled && e.getAttribute('aria-disabled') !== 'true');
   const score = e => {
     const values = [e.getAttribute('aria-label'), e.id, e.name, e.placeholder, e.title,
       e.getAttribute('data-testid'), e.innerText, e.value].map(normalize).filter(Boolean);
     let best = 0;
     values.forEach(value => {
       if (value === wanted) best = Math.max(best, 100);
       else if (value.startsWith(wanted)) best = Math.max(best, 70);
       else if (value.includes(wanted)) best = Math.max(best, 45);
     });
     return best;
   };
   const ranked = candidates.map(element => ({element,score:score(element)})).filter(row => row.score > 0)
     .sort((a,b) => b.score - a.score);
   if (!ranked.length) return {element:null,ambiguous:false};
   return {element:ranked[0].element,ambiguous:ranked.length > 1 && ranked[0].score === ranked[1].score};
 };
 const fingerprint = e => JSON.stringify([e.tagName,e.type,e.id,e.name,e.getAttribute('href'),
   label(e),e.getAttribute('role'),e.getAttribute('aria-disabled'),e.readOnly,ntSensitive(e) ? null : e.value,e.getAttribute('onclick'),
   e.form && e.form.action,e.closest('article,li,tr,[role=row]')?.innerText,
   e instanceof HTMLSelectElement ? Array.from(e.options).map(o=>[o.value,o.text,o.disabled]) : null]);
 const inViewport = e => {
   const r = e.getBoundingClientRect();
   return r.bottom > 0 && r.right > 0 && r.top < innerHeight && r.left < innerWidth;
 };
 // Cache only page text; mutations invalidate this cache, not unrelated element handles.
 let cache = globalThis.__ntBrowserTextCache;
 if (!cache || cache.url !== location.href) {
   if (cache) cache.observer.disconnect();
   cache = {url:location.href,dirty:true};
   cache.observer = new MutationObserver(()=>{cache.dirty=true;});
   cache.observer.observe(document.documentElement,{subtree:true,childList:true,attributes:true,characterData:true});
   globalThis.__ntBrowserTextCache = cache;
 }
 const snapshot = (error, code) => {
   const mode = error ? 'summary' : (args.mode || 'summary');
   if (!['summary','focused','full'].includes(mode)) return JSON.stringify({error:'不支持的读取模式',code:'invalid_mode'});
   const query = String(args.query || '').trim().toLowerCase();
   if (mode === 'focused' && !query) return JSON.stringify({error:'focused 模式需要 query',code:'invalid_query'});
   const offset = Number.isInteger(args.offset) && args.offset >= 0 && !error ? args.offset : 0;
   const root = document.body || document.documentElement;
   if (cache.width !== innerWidth || cache.height !== innerHeight) cache.dirty = true;
   if (cache.dirty) {
     const walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT);
     const chunks = []; let length = 0;
     while (walker.nextNode() && length < 200000) {
       const node = walker.currentNode, parent = node.parentElement;
       if (!parent || parent.closest('script,style,noscript,input,textarea,[contenteditable]') || !ntVisible(parent)) continue;
       const value = node.textContent.trim();
       if (value) { const text = value.slice(0,200000-length); chunks.push({text,parent}); length += text.length + 1; }
     }
     cache.chunks = chunks;
     cache.truncated = length >= 200000;
     cache.width = innerWidth; cache.height = innerHeight;
     cache.dirty = false;
   }
   const chunks = cache.chunks.filter(c => mode === 'full' ||
     (mode === 'focused' ? c.text.toLowerCase().includes(query) : inViewport(c.parent)));
   const text = chunks.map(c=> {
     if (mode !== 'focused') return c.text;
     const match = c.text.toLowerCase().indexOf(query);
     return c.text.slice(Math.max(0,match-240),match+query.length+600);
   }).join('\n');
   const content = text.slice(offset, offset + (mode === 'summary' ? 2500 : 6000));
   const candidates = interactiveElements()
     .filter(e=>ntVisible(e) && (mode === 'full' || (mode === 'focused' ? label(e).toLowerCase().includes(query) : inViewport(e))));
   const elementOffset = Number.isInteger(args.elementOffset) && args.elementOffset >= 0 && !error ? args.elementOffset : 0;
   const elements = candidates.slice(elementOffset,elementOffset+30);
   const id = Date.now().toString(36) + Math.random().toString(36).slice(2);
   const result = {snapshot:id,url:location.href.slice(0,2048),urlTruncated:location.href.length>2048,title:document.title.slice(0,200),mode,content,
     contentLength:text.length,sourceTruncated:cache.truncated,authenticationUiPresent:ntNeedsUser(),
     nextOffset:offset + content.length < text.length ? offset + content.length : null,
     elements:elements.map((e,i)=>({element:i+1,tag:e.tagName.toLowerCase(),type:e.type || '',
       label:label(e).slice(0,100),requiresUser:ntSensitive(e),disabled:!!(e.disabled || e.getAttribute('aria-disabled') === 'true'),
       options:e instanceof HTMLSelectElement ? Array.from(e.options).slice(0,40).map(o=>({text:o.text.slice(0,60),value:o.value})) : undefined}))};
   if (error) { result.error=error; result.code=code; result.actionExecuted=false; }
   for (const row of result.elements) {
     if (row.options) {
       const total = elements[row.element-1].options.length;
       while (row.options.length && JSON.stringify(row).length > 2000) row.options.pop();
       row.optionsOmitted = total - row.options.length;
     }
   }
   // Keep JSON below the shared tool-output limit, including large select option lists.
   while (result.elements.length && JSON.stringify(result).length > 17000) { result.elements.pop(); elements.pop(); }
   result.elementsOmitted = candidates.length - elements.length;
   result.nextElementOffset = elementOffset + elements.length < candidates.length ? elementOffset + elements.length : null;
   const viewKey = JSON.stringify([mode,mode === 'focused' ? query : '',offset]);
   if (!error && args.since && state && args.since === state.id && state.url === location.href && state.viewKey === viewKey) {
     if (state.content === content) { delete result.content; result.contentUnchanged=true; result.baseSnapshot=state.id; }
     else {
       let start=0, end=0;
       while (start < state.content.length && start < content.length && state.content[start] === content[start]) ++start;
       while (end < state.content.length-start && end < content.length-start && state.content[state.content.length-1-end] === content[content.length-1-end]) ++end;
       const patch = {start,deleteCount:state.content.length-start-end,text:content.slice(start,content.length-end)};
       if (JSON.stringify(patch).length + 100 < content.length) { delete result.content; result.contentPatch=patch; result.baseSnapshot=state.id; }
     }
   }
   state = {id,url:location.href,elements,fingerprints:elements.map(fingerprint),usable:true,content,viewKey};
   globalThis.__ntBrowserState = state;
   return JSON.stringify(result);
 };
 if (args.action === 'wait_for') {
   const match = resolveTarget(args.target || args.query || '');
   return JSON.stringify({waitFor:true,found:!!match.element,ambiguous:match.ambiguous,target:String(args.target || args.query || '')});
 }
 if (['click','fill','select'].includes(args.action)) {
   const semantic = String(args.target || '').trim();
   if (!semantic && (!state || !state.usable || args.snapshot !== state.id || state.url !== location.href))
     return snapshot('元素快照已失效，已附最新快照；请使用新编号决定下一步','stale_snapshot');
   const resolved = semantic ? resolveTarget(semantic) : {element:state.elements[args.element - 1],ambiguous:false};
   const e = resolved.element;
   if (resolved.ambiguous) return JSON.stringify({error:'目标匹配到多个元素，请提供更具体的 target 或使用 element 编号',code:'target_ambiguous'});
   if ((!semantic && !Number.isInteger(args.element)) || !e || !e.isConnected || !ntVisible(e) || e.disabled || e.closest('[inert]') || e.getAttribute('aria-disabled') === 'true')
     return snapshot('目标元素已不可操作，已附最新快照','target_unavailable');
   if (ntSensitive(e) || e.matches('input[type=file]')) return JSON.stringify({needsUser:true});
   if (!semantic && fingerprint(e) !== state.fingerprints[args.element - 1])
     return snapshot('目标元素含义已变化，已附最新快照；尚未执行操作','target_changed');
   if (args.action === 'click') { e.click(); }
   else if (args.action === 'select') {
     if (!(e instanceof HTMLSelectElement) || !Array.from(e.options).some(o => o.value === args.text))
       return JSON.stringify({error:'请选择有效的 select 选项值'});
     e.value = args.text;
     e.dispatchEvent(new Event('input',{bubbles:true})); e.dispatchEvent(new Event('change',{bubbles:true}));
   } else {
     if (!(e instanceof HTMLInputElement || e instanceof HTMLTextAreaElement) || e.readOnly ||
         (e instanceof HTMLInputElement && !['text','search','email','url','tel','number'].includes(e.type)))
       return JSON.stringify({error:'该元素不是可填写的普通输入框'});
     const proto = e instanceof HTMLInputElement ? HTMLInputElement.prototype : HTMLTextAreaElement.prototype;
     Object.getOwnPropertyDescriptor(proto,'value').set.call(e, args.text || '');
     e.dispatchEvent(new Event('input',{bubbles:true})); e.dispatchEvent(new Event('change',{bubbles:true}));
   }
   state.usable = false;
   cache.dirty = true;
   return JSON.stringify({acted:true});
 }
 if (args.action === 'scroll') {
   window.scrollBy(0,(args.text === 'up' ? -1 : 1)*innerHeight*0.8);
   return JSON.stringify({acted:true});
 }
 return snapshot();
})()
)JS";
}

struct BrowserPanel::Request {
    QMutex mutex;
    QWaitCondition ready;
    QString result;
    QJsonObject args;
    bool done = false;
    std::atomic_bool cancelled{false};
    QElapsedTimer elapsed;
    QString authenticationStatus;
};

BrowserPanel::BrowserPanel(QWidget *parent, int authenticationWaitMs)
    : QWidget(parent), authenticationWaitMs_(qMax(1000, authenticationWaitMs))
{
    setMinimumWidth(320);
    // 除 header 外整个区域都留给网页：不留外边距，也不放状态栏。
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto *header = new QWidget(this);
    auto *bar = new QHBoxLayout(header);
    bar->setContentsMargins(8, 6, 8, 6);
    bar->setSpacing(4);
    const QString navStyle = browserNavButtonStyle();
    auto navButton = [&](const QString &iconFile, const QString &tip) {
        auto *b = new QPushButton(this);
        b->setStyleSheet(navStyle);
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(tip);
        b->setIcon(browserNavIcon(iconFile));
        b->setIconSize(QSize(kNavIconPx, kNavIconPx));
        b->setFixedSize(kNavButtonPx, kNavButtonPx);
        return b;
    };
    auto *collapse = navButton(QStringLiteral("collapse.png"), QStringLiteral("收起浏览器，恢复对话窗口宽度"));
    auto *back = navButton(QStringLiteral("back.png"), QStringLiteral("后退"));
    auto *forward = navButton(QStringLiteral("forward.png"), QStringLiteral("前进"));
    auto *reload = navButton(QStringLiteral("refresh.png"), QStringLiteral("刷新页面"));
    auto *home = navButton(QStringLiteral("home.png"), QStringLiteral("回到主页"));
    address_ = new QLineEdit(this);
    address_->setPlaceholderText(QStringLiteral("搜索或输入网址"));
    address_->setFixedHeight(kAddressHeightPx);
    // 全局 QLineEdit 样式带 4px 上下内边距 + 1px 边框，配上 28px 的固定高度会把
    // 文字裁掉一半；这里收紧内边距，让 32px 里的可用高度足够放下 14px 的字。
    address_->setStyleSheet(QStringLiteral("QLineEdit{padding:2px 10px;}"));
    bar->addWidget(collapse);
    bar->addWidget(back);
    bar->addWidget(forward);
    bar->addWidget(reload);
    bar->addWidget(home);
    bar->addWidget(address_, 1);
    auto *external = navButton(QStringLiteral("open_external.png"), QStringLiteral("在系统浏览器中打开"));
    bar->addWidget(external);
    connect(collapse, &QPushButton::clicked, this, &BrowserPanel::collapseRequested);
    layout->addWidget(header);
    authenticationBar_ = new QWidget(this);
    auto *authenticationLayout = new QHBoxLayout(authenticationBar_);
    authenticationLayout->setContentsMargins(8, 4, 8, 4);
    authenticationCountdown_ = new QLabel(authenticationBar_);
    authenticationCountdown_->setObjectName(QStringLiteral("browserAuthenticationCountdown"));
    authenticationCountdown_->setTextFormat(Qt::PlainText);
    authenticationCountdown_->setWordWrap(true);
    authenticationLayout->addWidget(authenticationCountdown_, 1);
    auto *continueButton = new QPushButton(QStringLiteral("已完成，继续"), authenticationBar_);
    auto *skipButton = new QPushButton(QStringLiteral("跳过登录"), authenticationBar_);
    skipButton->setObjectName(QStringLiteral("browserSkipAuthentication"));
    authenticationLayout->addWidget(continueButton);
    authenticationLayout->addWidget(skipButton);
    connect(continueButton, &QPushButton::clicked, this, &BrowserPanel::resume);
    connect(skipButton, &QPushButton::clicked, this, &BrowserPanel::continueWithoutLogin);
    layout->addWidget(authenticationBar_);
    authenticationBar_->hide();
    view_ = new QWebEngineView(this);
    qApp->installEventFilter(this);
    profile_ = new QWebEngineProfile(this);
    view_->setPage(new BrowserPage(profile_, view_));
    view_->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, false);
    view_->settings()->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, false);
    installHomeThemeScript();
    layout->addWidget(view_, 1);
    loadHome();  // Home 按钮始终回到内置搜索起始页
    // 认证等待支持手动继续、跳过及无操作倒计时；停止仍由输入框统一触发。
    // 用户自己点这些按钮属于正常浏览，不打断 AI。
    connect(address_,&QLineEdit::returnPressed,this,[this]{ navigate(address_->text()); });
    connect(back,&QPushButton::clicked,this,[this]{ view_->back(); });
    connect(forward,&QPushButton::clicked,this,[this]{ view_->forward(); });
    connect(reload,&QPushButton::clicked,this,[this]{ view_->reload(); });
    connect(home,&QPushButton::clicked,this,[this]{ loadHome(); });
    connect(external,&QPushButton::clicked,this,[this]{
        const auto url = view_->url();
        if (url.scheme() == "http" || url.scheme() == "https") QDesktopServices::openUrl(url);
    });
    connect(view_,&QWebEngineView::urlChanged,this,[this](const QUrl &url){
        // 起始页是内部资源，地址栏留空更像一个正常的主页。
        address_->setText(url == browserHomeUrl() ? QString() : url.toDisplayString(QUrl::RemoveUserInfo));
        ++generation_;
    });
    connect(view_,&QWebEngineView::loadStarted,this,[this]{ loading_ = true; ++generation_; });
    connect(view_,&QWebEngineView::loadFinished,this,[this](bool ok){
        loading_ = false;
        settled_.start(); settleMs_ = 1200;
        if (!ok && request_ && !manual_) finish(QStringLiteral("网页加载失败，可点击刷新重试。"),true);
    });
    connect(view_->page(),&QWebEnginePage::renderProcessTerminated,this,[this]{ loading_ = false; finish(QStringLiteral("浏览器渲染进程退出，请刷新重试。"),true); });
    connect(view_->page(),&QWebEnginePage::newWindowRequested,this,[this](QWebEngineNewWindowRequest &r){
        // target=_blank 很常见，直接在本视图里打开，不必因此打断 AI。
        const auto url = r.requestedUrl();
        if (url.scheme() == "http" || url.scheme() == "https") view_->load(url);
    });
    connect(view_->page(),&QWebEnginePage::authenticationRequired,this,[this](const QUrl &, QAuthenticator *auth){
        if (authenticationWaitSkipped_) return;
        takeOver(QStringLiteral("网站需要 HTTP 身份认证，请输入账号和密码。"));
        bool ok = false;
        const QString user = QInputDialog::getText(this,QStringLiteral("网站认证"),QStringLiteral("账号"),QLineEdit::Normal,{},&ok);
        if (!ok) return;
        const QString password = QInputDialog::getText(this,QStringLiteral("网站认证"),QStringLiteral("密码"),QLineEdit::Password,{},&ok);
        if (ok) { auth->setUser(user); auth->setPassword(password); }
    });
    connect(view_->page(),&QWebEnginePage::permissionRequested,this,[this](QWebEnginePermission permission){
        if (authenticationWaitSkipped_) { permission.deny(); return; }
        takeOver(QStringLiteral("网站请求设备权限，请确认后继续。"));
        const auto answer = QMessageBox::question(this,QStringLiteral("网站权限"),
            QStringLiteral("允许 %1 使用%2？").arg(permission.origin().toDisplayString(),permissionLabel(permission.permissionType())),
            QMessageBox::Yes|QMessageBox::No,QMessageBox::No);
        if (answer == QMessageBox::Yes) permission.grant(); else permission.deny();
    });
    connect(view_->page(),&QWebEnginePage::webAuthUxRequested,this,[this](QWebEngineWebAuthUxRequest *request){
        if (authenticationWaitSkipped_) { request->cancel(); return; }
        takeOver(QStringLiteral("网站需要安全密钥或通行密钥认证，请完成认证后继续。"));
        QPointer<QWebEngineWebAuthUxRequest> guard(request);
        const auto handle = [this,guard]{
            if (!guard) return;
            using State = QWebEngineWebAuthUxRequest::WebAuthUxState;
            bool ok = false;
            if (guard->state() == State::SelectAccount) {
                const auto account = QInputDialog::getItem(this,QStringLiteral("通行密钥认证"),QStringLiteral("选择账号"),guard->userNames(),0,false,&ok);
                if (guard) { if (ok) guard->setSelectedAccount(account); else guard->cancel(); }
            } else if (guard->state() == State::CollectPin) {
                const auto pin = QInputDialog::getText(this,QStringLiteral("安全密钥认证"),QStringLiteral("输入安全密钥 PIN"),QLineEdit::Password,{},&ok);
                if (guard) { if (ok) guard->setPin(pin); else guard->cancel(); }
            } else if (guard->state() == State::RequestFailed) {
                // 没有底部状态栏了，认证失败这类结果直接发到对话里让用户看到。
                emit attentionRequired(QStringLiteral("安全密钥认证失败，请在网站重试或使用其他登录方式。"));
                guard->cancel();
            }
        };
        connect(request,&QWebEngineWebAuthUxRequest::stateChanged,this,[handle]{ handle(); },Qt::QueuedConnection);
        QTimer::singleShot(0,this,handle);
    });
    timer_ = new QTimer(this);
    timer_->setInterval(200);
    connect(timer_,&QTimer::timeout,this,[this]{
        if (request_ && request_->cancelled) { cancel(); return; }
        if (manual_) {
            if (request_) request_->elapsed.restart();
            updateAuthenticationCountdown();
            checkAutoResume();
            return;
        }
        if (!request_) return;
        if (request_->elapsed.elapsed() > 30000) { finish(QStringLiteral("浏览器操作超时，可点击刷新重试。"),true); view_->stop(); return; }
        if (!loading_ && !evaluating_ && (!settled_.isValid() || settled_.elapsed() >= settleMs_)) observe();
    });
    timer_->start();
}

BrowserPanel::~BrowserPanel()
{
    qApp->removeEventFilter(this);
    ++generation_;
    finish(QStringLiteral("浏览器已关闭"));
    {
        QMutexLocker lock(&queuedRequestsMutex_);
        for (const auto &request : std::as_const(queuedRequests_)) {
            if (!request) continue;
            request->cancelled = true;
            QMutexLocker requestLock(&request->mutex);
            request->result = QStringLiteral("浏览器已关闭");
            request->done = true;
            request->ready.wakeAll();
        }
        queuedRequests_.clear();
    }
    delete view_; // page must die before its profile
    delete profile_;
}

QString BrowserPanel::execute(const QJsonObject &args, LlmTools::ToolAbort *abort)
{
    if (QThread::currentThread() == thread()) return QStringLiteral("浏览工具须由 Agent 工作线程调用");
    auto request = std::make_shared<Request>();
    {
        QMutexLocker lock(&queuedRequestsMutex_);
        queuedRequests_.append(request);
    }
    QMetaObject::invokeMethod(this,[this,args,request]{
        if (!request->cancelled) start(args,request);
        // Keep the request queued until start() has installed request_. This
        // closes the destruction race between dequeue and start.
        QMutexLocker lock(&queuedRequestsMutex_);
        queuedRequests_.removeOne(request);
    },Qt::QueuedConnection);
    QMutexLocker lock(&request->mutex);
    while (!request->done) {
        if (abort && abort->isAborted()) {
            request->cancelled = true;
            // Wake the GUI-side state machine as well. Returning directly from
            // the worker must not leave manual/authentication state attached
            // to the next browser request.
            QMetaObject::invokeMethod(this, [this, request] {
                if (request_ == request) cancel();
            }, Qt::QueuedConnection);
            return QStringLiteral("浏览器操作已取消");
        }
        request->ready.wait(&request->mutex,50);
    }
    return request->result;
}

void BrowserPanel::start(const QJsonObject &args, const std::shared_ptr<Request> &request)
{
    finish(QStringLiteral("浏览器操作已被替换"));
    request_ = request;
    request_->args = args;
    request_->elapsed.start();
    emit activityRequested();
    const QString action = args.value("action").toString();
    if (!QStringList{"open","read","click","fill","select","scroll","back","wait_for","wait_user"}.contains(action)) {
        finish(QStringLiteral("不支持的浏览器操作")); return;
    }
    if (manual_) return;
    if (action == "wait_user") {
        if (authenticationWaitSkipped_) {
            request_->authenticationStatus = QStringLiteral("skipped");
            request_->args = QJsonObject{{"action","read"}};
            settled_.start();
            settleMs_ = 0;
        } else {
            const QString reason = args.value("reason").toString().trimmed();
            if (reason.isEmpty()) { finish(QStringLiteral("wait_user 需要 reason：请说明任务为何必须登录；仅出现登录组件不构成理由。")); return; }
            takeOver(reason.left(500));
        }
        return;
    }
    if (action == "open") {
        const QUrl url(args.value("url").toString());
        if (!url.isValid() || url.host().isEmpty() || !url.userInfo().isEmpty() || (url.scheme() != "https" && url.scheme() != "http")) {
            finish(QStringLiteral("仅支持不含账号密码的有效 HTTP/HTTPS 地址")); return;
        }
        request_->args = QJsonObject{{"action","read"}};
        view_->load(url);
    } else if (action == "back") {
        request_->args = QJsonObject{{"action","read"}};
        view_->back();
    }
}

void BrowserPanel::navigate(const QString &text)
{
    const QString input = text.trimmed();
    if (input.isEmpty()) return;

    // 明显的网址沿用浏览器补全；其余内容作为搜索词，避免把“Qt 文档”之类的
    // 输入误判成本地域名。显式协议始终按网址处理，随后仍由安全检查兜底。
    const bool looksLikeUrl = input.contains(QStringLiteral("://"))
        || (!input.contains(QRegularExpression(QStringLiteral("\\s")))
            && (input.contains(QLatin1Char('.')) || input.startsWith(QStringLiteral("localhost"))));
    QUrl url;
    if (looksLikeUrl) {
        url = QUrl::fromUserInput(input);
    } else {
        url = QUrl(QStringLiteral("https://www.bing.com/search"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("q"), input);
        url.setQuery(query);
    }
    if ((url.scheme() == "http" || url.scheme() == "https") && url.userInfo().isEmpty()) view_->load(url);
}

void BrowserPanel::loadHome()
{
    // 走一次真正的导航，而不是 setHtml：Home 按钮因此和普通浏览一样有历史记录、
    // 可刷新，也不会出现“已经在主页上再点一次看不出变化”之外的各种歧义。
    view_->load(browserHomeUrl());
}

// 起始页跟随应用主题。WebEngine 的 prefers-color-scheme 读的是系统设置，
// 和应用当前主题不一定一致，所以这里由应用把当前主题写进 <html>。
void BrowserPanel::installHomeThemeScript()
{
    // 必须在 DocumentReady 注入：文档创建时 <html> 还没被解析出来，那时设的属性
    // 会被解析器新建的元素整个丢掉（实测仍停在 HTML 里写死的 light）。
    // 带上 URL 判断，免得给用户访问的每个网站都塞一个 data- 属性。
    const QString source = QStringLiteral(
        "if(location.href.indexOf('qrc:/html/browser-home.html')===0){"
        "document.documentElement.setAttribute('data-nt-theme','%1');}")
        .arg(browserIsDarkTheme() ? QStringLiteral("dark") : QStringLiteral("light"));

    QWebEngineScript script;
    script.setName(QStringLiteral("ntHomeTheme"));
    script.setInjectionPoint(QWebEngineScript::DocumentReady);
    // 主世界：属性要参与页面的 CSS 匹配。
    script.setWorldId(QWebEngineScript::MainWorld);
    script.setRunsOnSubFrames(false);
    script.setSourceCode(source);

    const auto existing = profile_->scripts()->find(QStringLiteral("ntHomeTheme"));
    for (const auto &old : existing) {
        profile_->scripts()->remove(old);
    }
    profile_->scripts()->insert(script);
}

void BrowserPanel::observe()
{
    if (!request_ || manual_) return;
    evaluating_ = true;
    const auto request = request_;
    const auto generation = generation_;
    const auto source = QString::fromUtf8(script)
        .arg(QString::fromUtf8(authDetection),
             QString::fromUtf8(QJsonDocument(request->args).toJson(QJsonDocument::Compact)));
    QPointer<BrowserPanel> guard(this);
    view_->page()->runJavaScript(source,QWebEngineScript::ApplicationWorld,[this,guard,request,generation](const QVariant &value){
        if (!guard) return;
        evaluating_ = false;
        if (request_ != request || manual_ || request->cancelled) return;
        if (generation != generation_) { request_->args = QJsonObject{{"action","read"}}; return; }
        const QString result = value.toString();
        auto object = QJsonDocument::fromJson(result.toUtf8()).object();
        if (object.isEmpty()) qWarning() << "BrowserPanel: empty browser script result for action" << request->args.value("action").toString() << result;
        if (object.value("needsUser").toBool()) {
            if (authenticationWaitSkipped_) {
                request_->authenticationStatus = QStringLiteral("skipped");
                request_->args = QJsonObject{{"action","read"}};
                settled_.start();
                settleMs_ = 0;
            } else {
                takeOver(QStringLiteral("当前操作涉及认证或需要本人填写的字段，请在右侧处理。"));
            }
        } else if (object.value("waitFor").toBool()) {
            const int timeoutMs = qBound(250, request_->args.value("timeoutMs").toInt(10000), 30000);
            if (object.value("found").toBool()) {
                request_->args = QJsonObject{{"action", "read"}, {"mode", "summary"}};
                settled_.start();
                settleMs_ = 0;
            } else if (request->elapsed.elapsed() >= timeoutMs) {
                finish(QString::fromUtf8(QJsonDocument(QJsonObject{
                    {QStringLiteral("error"), QStringLiteral("等待目标超时")},
                    {QStringLiteral("code"), QStringLiteral("wait_timeout")},
                    {QStringLiteral("target"), object.value("target")}
                }).toJson(QJsonDocument::Compact)));
            } else {
                // 保持同一个 wait_for 请求，定时器会在短间隔后重新检查 DOM。
                settled_.start();
                settleMs_ = 250;
            }
        } else if (object.value("acted").toBool()) {
            request_->args = QJsonObject{{"action","read"},{"since",request_->args.value("snapshot")}};
            settled_.start(); settleMs_ = 350;
        } else if (!object.isEmpty()) {
            if (!request->authenticationStatus.isEmpty()) {
                object.insert(QStringLiteral("authenticationStatus"), request->authenticationStatus);
                object.insert(QStringLiteral("guidance"), request->authenticationStatus == QStringLiteral("resumed")
                    ? QStringLiteral("认证等待已结束，请核对当前页面并继续任务，不假设登录成功，也不要因残留登录组件再次暂停。")
                    : QStringLiteral("请根据当前页面继续任务，不假设登录成功。本轮不再请求等待登录；优先公开页面、访客入口或其他公开来源，不绕过访问控制。若确实必须登录，说明已完成部分、受限原因及用户可执行的建议。"));
            }
            finish(QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)));
        }
    });
}

void BrowserPanel::takeOver(const QString &reason)
{
    if (authenticationWaitSkipped_) return;
    const bool notify = !manual_;
    if (!manual_) manualWait_.start();
    manual_ = true;
    authenticationBar_->show();
    updateAuthenticationCountdown();
    ++generation_;
    if (notify) emit attentionRequired(reason + QStringLiteral(" 无操作 %1 秒后将尝试未登录方式继续，也可点击“跳过登录”。").arg(authenticationWaitMs_ / 1000));
}

void BrowserPanel::resume()
{
    manual_ = false;
    authenticationBar_->hide();
    ++generation_;
    if (request_) { request_->args = QJsonObject{{"action","read"}}; request_->elapsed.restart(); request_->authenticationStatus = QStringLiteral("resumed"); }
}

void BrowserPanel::continueWithoutLogin()
{
    const bool timedOut = manualWait_.isValid() && manualWait_.elapsed() >= authenticationWaitMs_;
    resume();
    authenticationWaitSkipped_ = true;
    if (request_) request_->authenticationStatus = timedOut ? QStringLiteral("timed_out") : QStringLiteral("skipped");
    for (auto *dialog : findChildren<QInputDialog *>()) dialog->reject();
    for (auto *dialog : findChildren<QMessageBox *>()) dialog->reject();
}

void BrowserPanel::resetAuthenticationWait()
{
    authenticationWaitSkipped_ = false;
}

void BrowserPanel::updateAuthenticationCountdown()
{
    const qint64 remaining = qMax<qint64>(0, authenticationWaitMs_ - manualWait_.elapsed());
    authenticationCountdown_->setText(QStringLiteral("等待你完成认证：%1 秒后尝试未登录方式继续。网页操作会重新计时。").arg((remaining + 999) / 1000));
}

bool BrowserPanel::eventFilter(QObject *watched, QEvent *event)
{
    auto *widget = qobject_cast<QWidget *>(watched);
    if (manual_ && widget && isAncestorOf(widget) &&
        (event->type() == QEvent::KeyPress || event->type() == QEvent::MouseButtonPress ||
         event->type() == QEvent::Wheel || event->type() == QEvent::TouchBegin)) {
        manualWait_.restart();
        updateAuthenticationCountdown();
    }
    return QWidget::eventFilter(watched, event);
}

// 页面离开认证环节后自动把控制权交还 AI，无需任何按钮。
void BrowserPanel::checkAutoResume()
{
    if (!manual_) return;
    if (manualWait_.isValid() && manualWait_.elapsed() >= authenticationWaitMs_) {
        continueWithoutLogin();
        return;
    }
    if (!request_ || evaluating_ || loading_) return;
    // 刚交出控制权时先让用户动手，别立刻抢回来。
    if (!manualWait_.isValid() || manualWait_.elapsed() < kManualMinWaitMs) return;
    if (settled_.isValid() && settled_.elapsed() < settleMs_) return;
    evaluating_ = true;
    const auto request = request_;
    const auto generation = generation_;
    QPointer<BrowserPanel> guard(this);
    const auto source = QString::fromUtf8(authProbe).arg(QString::fromUtf8(authDetection));
    view_->page()->runJavaScript(source,QWebEngineScript::ApplicationWorld,[this,guard,request,generation](const QVariant &value){
        if (!guard) return;
        evaluating_ = false;
        if (!manual_ || request_ != request || request->cancelled) return;
        if (generation != generation_) return;
        if (value.toBool()) return;  // 仍在认证页，继续等用户
        resume();
    });
}

void BrowserPanel::cancel()
{
    finish(QStringLiteral("浏览器操作已取消，不要自动重试，等待用户指示。"));
    view_->stop();
    // 取消后不再挂起，否则用户的下一条指令会被卡在“等待接管”上。
    manual_ = false;
    authenticationWaitSkipped_ = false;
    authenticationBar_->hide();
}

void BrowserPanel::clearSnapshotCache()
{
    resetAuthenticationWait();
    view_->page()->runJavaScript(QStringLiteral(
        "if(globalThis.__ntBrowserTextCache) globalThis.__ntBrowserTextCache.observer.disconnect();"
        "delete globalThis.__ntBrowserTextCache; delete globalThis.__ntBrowserState;"),
        QWebEngineScript::ApplicationWorld);
}

// notifyUser 用于失败、超时等终态：面板底部已经没有状态栏，这些结果改成发一条
// 对话消息，否则用户只会看到操作无声无息地停掉。
void BrowserPanel::finish(const QString &result, bool notifyUser)
{
    if (!request_) return;
    auto request = std::move(request_);
    QMutexLocker lock(&request->mutex);
    request->result = result;
    request->done = true;
    request->ready.wakeAll();
    if (notifyUser) emit attentionRequired(result);
}

