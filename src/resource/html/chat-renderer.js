(function (global) {
  const ChatApp = global.ChatApp;
  const state = ChatApp.state;
  const constants = ChatApp.constants;
  const helpers = ChatApp.helpers;
  const actions = ChatApp.actions;
  const markdown = ChatApp.markdown;

  function scrollContainer() {
    return document.scrollingElement || document.documentElement;
  }

  function isMessageStreamingIncomplete(message) {
    const c = message.completed;
    return c === false || c === 0 || c === 'false';
  }

  function isNearBottom() {
    const el = scrollContainer();
    if (!el) {
      return true;
    }
    const distance = el.scrollHeight - (el.scrollTop + el.clientHeight);
    return distance <= constants.AUTO_SCROLL_THRESHOLD;
  }

  const pendingMessageUpdates = Object.create(null);
  let messageUpdateFrameId = 0;
  let messageUpdateTimerId = 0;
  const STREAMING_CODE_UPDATE_INTERVAL_MS = 120;

  function scrollPageToBottom() {
    const root = scrollContainer();
    if (!root) {
      return;
    }
    root.scrollTop = Math.max(0, root.scrollHeight - root.clientHeight);
    requestAnimationFrame(function () {
      updateChatScrollChrome();
    });
  }

  function updateStreamingTailsVisibility() {
    const hide = isNearBottom();
    const tails = document.querySelectorAll('.bubble-streaming-tail');
    for (let i = 0; i < tails.length; i++) {
      if (hide) {
        tails[i].classList.add('bubble-streaming-tail--hidden');
        tails[i].setAttribute('aria-hidden', 'true');
        tails[i].setAttribute('tabindex', '-1');
      } else {
        tails[i].classList.remove('bubble-streaming-tail--hidden');
        tails[i].setAttribute('aria-hidden', 'false');
        tails[i].setAttribute('tabindex', '0');
      }
    }
  }

  function hasVerticalScrollbar() {
    const el = scrollContainer();
    if (!el) {
      return false;
    }
    return el.scrollHeight > el.clientHeight + 2;
  }

  function updateScrollBottomFabVisibility() {
    const btn = state.scrollBottomFab;
    if (!btn) {
      return;
    }
    const show = hasVerticalScrollbar() && !isNearBottom();
    btn.classList.toggle('chat-scroll-bottom-btn--visible', show);
    btn.setAttribute('aria-hidden', show ? 'false' : 'true');
  }

  function updateChatScrollChrome() {
    updateStreamingTailsVisibility();
    updateScrollBottomFabVisibility();
  }

  let chatScrollChromeListenersAttached = false;
  function ensureChatScrollChromeListeners() {
    if (chatScrollChromeListenersAttached) {
      return;
    }
    chatScrollChromeListenersAttached = true;
    const onScrollOrResize = function () {
      updateChatScrollChrome();
    };
    window.addEventListener('scroll', onScrollOrResize, { passive: true });
    window.addEventListener('resize', onScrollOrResize, { passive: true });
  }

  function ensureStreamingTailScrollListener() {
    ensureChatScrollChromeListeners();
  }

  function scheduleScrollChromeUpdate() {
    requestAnimationFrame(function () {
      updateChatScrollChrome();
    });
  }

  function initScrollBottomFab() {
    if (state.scrollBottomFab) {
      return;
    }
    const btn = document.createElement('button');
    btn.type = 'button';
    btn.className = 'chat-scroll-bottom-btn';
    btn.setAttribute('aria-label', '滚动到底部');
    btn.setAttribute('title', '滚动到底部');
    btn.setAttribute('aria-hidden', 'true');
    btn.innerHTML = ChatApp.icons.chevronDown;
    btn.addEventListener('click', function () {
      scrollPageToBottom();
    });
    document.body.appendChild(btn);
    state.scrollBottomFab = btn;
    ensureChatScrollChromeListeners();
    scheduleScrollChromeUpdate();
  }

  function scrollMessageToTop(node) {
    if (!node) {
      return;
    }

    function scrollNow() {
      const rect = node.getBoundingClientRect();
      const docEl = document.documentElement;
      const body = document.body;
      const y = rect.top + (window.pageYOffset || docEl.scrollTop || body.scrollTop || 0);
      window.scrollTo(0, y);
    }

    requestAnimationFrame(function () {
      scrollNow();
      requestAnimationFrame(scrollNow);
    });
  }

  function canInsertIntoActiveTurn() {
    return state.currentTurnEl && state.turnFillEl && state.turnFillEl.parentNode === state.currentTurnEl;
  }

  /**
   * 发起新一轮用户提问时，收起上一轮：去掉 min-height 与底部 fill（仅此时改变历史轮高度）。
   * 流式结束不再动 DOM，避免「答完后整块布局变化」。
   */
  function finalizePreviousTurn() {
    // 保留上一轮 DOM 中的工具条；仅断开引用，避免新一轮 ensureToolActivityHost 误复用旧节点
    state.toolActivityHost = null;
    if (!state.currentTurnEl) {
      return;
    }
    state.currentTurnEl.classList.remove('chat-turn--active');
    if (state.turnFillEl && state.turnFillEl.parentNode) {
      state.turnFillEl.parentNode.removeChild(state.turnFillEl);
    }
    state.currentTurnEl = null;
    state.turnFillEl = null;
  }

  function createActiveTurnShell(userNode) {
    finalizePreviousTurn();
    const turn = document.createElement('div');
    turn.className = 'chat-turn chat-turn--active';
    const fill = document.createElement('div');
    fill.className = 'chat-turn-fill';
    fill.setAttribute('aria-hidden', 'true');
    turn.appendChild(userNode);
    turn.appendChild(fill);
    state.messageList.appendChild(turn);
    state.currentTurnEl = turn;
    state.turnFillEl = fill;
    return turn;
  }

  function insertIntoActiveTurn(node) {
    if (canInsertIntoActiveTurn()) {
      state.currentTurnEl.insertBefore(node, state.turnFillEl);
      return;
    }
    state.messageList.appendChild(node);
  }

  function setupUserCollapse(wrapper) {
    if (!wrapper || !wrapper.classList.contains('user')) {
      return;
    }

    const bubble = wrapper.querySelector('.bubble');
    const content = bubble ? bubble.querySelector('.content') : null;
    if (!bubble || !content) {
      return;
    }

    const oldToggle = bubble.querySelector('.bubble-toggle-btn');
    if (oldToggle) {
      oldToggle.remove();
    }

    bubble.classList.remove('has-toggle');
    content.classList.remove('collapsible', 'is-collapsed');
    content.style.maxHeight = '';

    if (bubble.querySelector('.image-wrap')) {
      return;
    }

    if (content.scrollHeight <= constants.USER_COLLAPSE_MAX_HEIGHT + 4) {
      return;
    }

    content.classList.add('collapsible', 'is-collapsed');
    content.style.maxHeight = constants.USER_COLLAPSE_MAX_HEIGHT + 'px';
    bubble.classList.add('has-toggle');

    const toggleBtn = actions.makeToggleButton(false);
    toggleBtn.addEventListener('click', function (e) {
      e.stopPropagation();
      const expand = content.classList.contains('is-collapsed');
      if (expand) {
        content.classList.remove('is-collapsed');
        content.style.maxHeight = content.scrollHeight + 'px';
      } else {
        content.classList.add('is-collapsed');
        content.style.maxHeight = constants.USER_COLLAPSE_MAX_HEIGHT + 'px';
      }
      actions.syncToggleButton(toggleBtn, expand);
    });
    bubble.appendChild(toggleBtn);
  }

  function patchPreInPlace(oldPre, newPre) {
    const oldCode = oldPre.querySelector('code');
    const newCode = newPre.querySelector('code');
    if (oldCode && newCode) {
      if (oldCode.className !== newCode.className) {
        oldCode.className = newCode.className;
      }
      const nextText = newCode.textContent || '';
      if (oldCode.textContent !== nextText) {
        oldCode.textContent = nextText;
      }
      return;
    }

    const copyBtn = oldPre.querySelector('.code-copy-btn');
    if (copyBtn) {
      copyBtn.remove();
    }
    oldPre.innerHTML = newPre.innerHTML;
    if (copyBtn) {
      oldPre.appendChild(copyBtn);
    }
  }

  function patchListInPlace(oldList, newList) {
    const oldItems = Array.from(oldList.children);
    const newItems = Array.from(newList.children);
    let i = 0;
    for (; i < Math.min(oldItems.length, newItems.length); i++) {
      if (oldItems[i].tagName !== newItems[i].tagName) {
        return false;
      }
      if (!patchElementInPlace(oldItems[i], newItems[i])) {
        return false;
      }
    }
    while (oldList.children.length > i) {
      oldList.removeChild(oldList.lastChild);
    }
    for (; i < newItems.length; i++) {
      oldList.appendChild(newItems[i].cloneNode(true));
    }
    return true;
  }

  function patchElementInPlace(oldEl, newEl) {
    if (oldEl.tagName !== newEl.tagName) {
      return false;
    }
    if (oldEl.tagName === 'PRE') {
      patchPreInPlace(oldEl, newEl);
      return true;
    }
    if (oldEl.tagName === 'UL' || oldEl.tagName === 'OL') {
      return patchListInPlace(oldEl, newEl);
    }
    if (oldEl.innerHTML !== newEl.innerHTML) {
      oldEl.innerHTML = newEl.innerHTML;
    }
    return true;
  }

  function syncStreamingContentHtml(content, nextHtml) {
    const temp = document.createElement('div');
    temp.innerHTML = nextHtml;

    const oldChildren = Array.from(content.children);
    const newChildren = Array.from(temp.children);
    let index = 0;

    for (; index < Math.min(oldChildren.length, newChildren.length); index++) {
      if (!patchElementInPlace(oldChildren[index], newChildren[index])) {
        break;
      }
    }

    while (content.children.length > index) {
      content.removeChild(content.lastChild);
    }
    for (; index < newChildren.length; index++) {
      content.appendChild(newChildren[index].cloneNode(true));
    }

    content.dataset.renderedHtml = nextHtml;
    delete content.dataset.renderedText;
    enhanceCodeBlocks(content);
  }

  function enhanceCodeBlocks(content) {
    if (!content) {
      return;
    }

    const blocks = content.querySelectorAll('pre');
    blocks.forEach(function (pre) {
      const existingBtn = pre.querySelector('.code-copy-btn');
      if (existingBtn) {
        return;
      }

      const copyBtn = actions.makeCodeCopyButton();
      pre.appendChild(copyBtn);
    });
  }

  function resetPendingMessageUpdates() {
    Object.keys(pendingMessageUpdates).forEach(function (id) {
      delete pendingMessageUpdates[id];
    });
    if (messageUpdateFrameId) {
      cancelAnimationFrame(messageUpdateFrameId);
      messageUpdateFrameId = 0;
    }
    if (messageUpdateTimerId) {
      clearTimeout(messageUpdateTimerId);
      messageUpdateTimerId = 0;
    }
  }

  function ensureMessageParts(bubble) {
    let content = bubble.querySelector('.content');
    if (!content) {
      content = document.createElement('div');
      content.className = 'content';
      bubble.appendChild(content);
    }

    let imageWrap = bubble.querySelector('.image-wrap');
    if (!imageWrap) {
      imageWrap = document.createElement('div');
      imageWrap.className = 'image-wrap';
      bubble.appendChild(imageWrap);
    }

    return {
      content: content,
      imageWrap: imageWrap
    };
  }

  function setContentHtml(content, html, isStreaming) {
    const nextHtml = html || '';
    if (content.dataset.renderedHtml === nextHtml) {
      return;
    }

    if (isStreaming && content.dataset.renderedHtml) {
      syncStreamingContentHtml(content, nextHtml);
      return;
    }

    content.innerHTML = nextHtml;
    content.dataset.renderedHtml = nextHtml;
    delete content.dataset.renderedText;
    enhanceCodeBlocks(content);
  }

  function setContentText(content, text) {
    const nextText = text || '';
    if (content.dataset.renderedText === nextText) {
      return;
    }
    content.textContent = nextText;
    content.dataset.renderedText = nextText;
    delete content.dataset.renderedHtml;
  }

  function syncMessageContent(parts, message) {
    if (message.text) {
      parts.content.hidden = false;
      if (message.role === 'user') {
        setContentText(parts.content, message.text);
      } else {
        const streaming = isMessageStreamingIncomplete(message);
        setContentHtml(parts.content, markdown.renderMarkdown(message.text), streaming);
      }
    } else {
      parts.content.hidden = true;
      setContentHtml(parts.content, '');
    }
  }

  function syncMessageImages(parts, imageDataUrls, roleLabel) {
    if (!imageDataUrls.length) {
      parts.imageWrap.hidden = true;
      parts.imageWrap.innerHTML = '';
      return;
    }

    const previousKey = parts.imageWrap.dataset.imagesKey || '';
    const nextKey = JSON.stringify(imageDataUrls);
    if (previousKey !== nextKey) {
      parts.imageWrap.innerHTML = '';
      imageDataUrls.forEach(function (url, index) {
        const image = document.createElement('img');
        image.src = url;
        image.alt = (roleLabel || 'image') + ' ' + (index + 1);
        parts.imageWrap.appendChild(image);
      });
      parts.imageWrap.dataset.imagesKey = nextKey;
    }
    parts.imageWrap.hidden = false;
  }

  function syncUserActionLeft(wrapper, bubble, hasContent) {
    let actionLeft = wrapper.querySelector('.msg-action-left');
    if (!hasContent) {
      if (actionLeft) {
        actionLeft.remove();
      }
      return;
    }

    if (!actionLeft) {
      actionLeft = document.createElement('div');
      actionLeft.className = 'msg-action-left';
      actionLeft.appendChild(actions.makeCopyButton(function () {
        return wrapper.dataset.rawText || '';
      }));
      wrapper.insertBefore(actionLeft, bubble);
    }
  }

  function syncAssistantActionBar(bubble, wrapper, message, usage, hasContent) {
    let actionBar = bubble.querySelector('.bubble-actions');
    const shouldShow = (message.role === 'assistant' || message.role === 'error')
      && hasContent
      && !isMessageStreamingIncomplete(message)
      && !message.hideBubbleActions;

    if (!shouldShow) {
      if (actionBar) {
        actionBar.remove();
      }
      return;
    }

    if (!actionBar) {
      actionBar = document.createElement('div');
      actionBar.className = 'bubble-actions';
      actionBar.appendChild(actions.makeCopyButton(function () {
        return wrapper.dataset.rawText || '';
      }));
      actionBar.appendChild(actions.makeRetryButton());
      bubble.appendChild(actionBar);
    }

    let usageMeta = actionBar.querySelector('.bubble-actions-usage');
    const parts = [];
    if (usage && Number.isFinite(usage.inputTokens)) {
      parts.push('In ' + usage.inputTokens);
    }
    if (usage && Number.isFinite(usage.outputTokens)) {
      parts.push('Out ' + usage.outputTokens);
    }

    if (!parts.length) {
      if (usageMeta) {
        usageMeta.remove();
      }
      return;
    }

    if (!usageMeta) {
      usageMeta = document.createElement('span');
      usageMeta.className = 'bubble-actions-usage';
      actionBar.appendChild(usageMeta);
    }
    usageMeta.textContent = parts.join(' · ');
  }

  function syncStreamingTail(bubble, message) {
    const tailRole = message.role || 'assistant';
    let tail = bubble.querySelector('.bubble-streaming-tail');
    const role = message.role || 'assistant';
    const streamingIncomplete = (role === 'assistant' || role === 'error') && isMessageStreamingIncomplete(message);
    if (tailRole !== 'assistant' && tailRole !== 'error') {
      if (tail) {
        tail.remove();
      }
      return;
    }

    if (!streamingIncomplete) {
      if (tail) {
        tail.remove();
      }
      return;
    }

    if (!tail) {
      tail = document.createElement('div');
      tail.className = 'bubble-streaming-tail';
      tail.setAttribute('role', 'button');
      tail.setAttribute('tabindex', '0');
      tail.setAttribute('title', '点击滚动到底部');
      const dots = document.createElement('div');
      dots.className = 'typing-dots';
      for (let i = 0; i < 3; i++) {
        const dot = document.createElement('span');
        dot.className = 'dot';
        dots.appendChild(dot);
      }
      tail.appendChild(dots);
      tail.addEventListener('click', function (e) {
        e.stopPropagation();
        scrollPageToBottom();
      });
      tail.addEventListener('keydown', function (e) {
        if (e.key === 'Enter' || e.key === ' ') {
          e.preventDefault();
          e.stopPropagation();
          scrollPageToBottom();
        }
      });
      bubble.appendChild(tail);
    }

    ensureStreamingTailScrollListener();
    updateStreamingTailsVisibility();
  }

  function fillMessageElement(wrapper, bubble, message) {
    const role = message.role || 'assistant';
    const streamingIncomplete = (role === 'assistant' || role === 'error') && isMessageStreamingIncomplete(message);
    let className = 'message ' + role;
    if (streamingIncomplete) {
      className += ' streaming';
    }
    wrapper.className = className;
    wrapper.dataset.messageId = message.id || '';
    wrapper.dataset.rawText = message.text || '';

    const imageDataUrls = Array.isArray(message.imageDataUrls) && message.imageDataUrls.length
      ? message.imageDataUrls
      : (message.imageDataUrl ? [message.imageDataUrl] : []);
    const usage = message && message.usage && typeof message.usage === 'object'
      ? message.usage
      : null;
    const hasContent = !!(message.text || imageDataUrls.length);
    const parts = ensureMessageParts(bubble);

    syncMessageContent(parts, message);
    syncMessageImages(parts, imageDataUrls, message.roleLabel);

    if (message.role === 'user') {
      syncUserActionLeft(wrapper, bubble, hasContent);
      const actionBar = bubble.querySelector('.bubble-actions');
      if (actionBar) {
        actionBar.remove();
      }
    } else {
      const actionLeft = wrapper.querySelector('.msg-action-left');
      if (actionLeft) {
        actionLeft.remove();
      }
      syncAssistantActionBar(bubble, wrapper, message, usage, hasContent);
    }

    syncStreamingTail(bubble, message);
  }

  function createMessageElement(message) {
    const wrapper = document.createElement('div');
    wrapper.className = 'message ' + (message.role || 'assistant');
    wrapper.dataset.messageId = message.id || '';

    const bubble = document.createElement('div');
    bubble.className = 'bubble';
    wrapper.appendChild(bubble);

    fillMessageElement(wrapper, bubble, message);
    return wrapper;
  }

  function removeMessageById(id) {
    const node = state.messageList.querySelector('[data-message-id="' + id + '"]');
    if (node && node.parentNode) {
      node.parentNode.removeChild(node);
    }
  }

  const TOOL_ARGS_MAX = 800;
  const TOOL_RESULT_MAX = 1000;
  const TOOL_SUMMARY_LINE_MAX = 96;
  const toolResultCache = new Map();
  let nextToolResultId = 0;

  function clearToolResultCache() {
    toolResultCache.clear();
    nextToolResultId = 0;
  }

  function truncateToolText(s, max) {
    const t = String(s || '');
    if (t.length <= max) {
      return t;
    }
    return t.slice(0, max) + '\n…';
  }

  function oneLinePreview(s, max) {
    const t = String(s || '').replace(/\s+/g, ' ').trim();
    if (t.length <= max) {
      return t;
    }
    return t.slice(0, max - 1) + '…';
  }

  function buildToolSummaryLine(toolName, tail) {
    const name = toolName || 'tool';
    const t = String(tail || '').replace(/\s+/g, ' ').trim();
    if (!t) {
      return name;
    }
    return oneLinePreview(name + ' · ' + t, TOOL_SUMMARY_LINE_MAX);
  }

  function syncToolCardToggleUi(card) {
    const btn = card.querySelector('.chat-tool-card-toggle');
    const chev = card.querySelector('.chat-tool-card-chevron');
    const collapsed = card.classList.contains('chat-tool-card--collapsed');
    if (chev) {
      chev.textContent = collapsed ? '▶' : '▼';
    }
    if (btn) {
      btn.setAttribute('aria-expanded', collapsed ? 'false' : 'true');
    }
  }

  function wireToolCardToggle(card) {
    const btn = card.querySelector('.chat-tool-card-toggle');
    if (!btn || btn.dataset.wired === '1') {
      return;
    }
    btn.dataset.wired = '1';
    syncToolCardToggleUi(card);
    btn.addEventListener('click', function (e) {
      e.preventDefault();
      card.classList.toggle('chat-tool-card--collapsed');
      syncToolCardToggleUi(card);
    });
  }

  function setToolCardSummary(card, line, fullHint) {
    const el = card.querySelector('.chat-tool-card-summary');
    if (!el) {
      return;
    }
    el.textContent = line;
    if (fullHint && fullHint.length > 0) {
      el.setAttribute('title', fullHint);
    } else {
      el.removeAttribute('title');
    }
  }

  function clearToolActivity() {
    clearToolResultCache();
    if (state.toolActivityHost && state.toolActivityHost.parentNode) {
      state.toolActivityHost.parentNode.removeChild(state.toolActivityHost);
    }
    state.toolActivityHost = null;
  }

  function ensureToolActivityHost() {
    if (state.toolActivityHost && state.toolActivityHost.parentNode) {
      return state.toolActivityHost;
    }
    const host = document.createElement('div');
    host.className = 'chat-tool-activity-host';
    host.setAttribute('aria-live', 'polite');
    insertIntoActiveTurn(host);
    state.toolActivityHost = host;
    return host;
  }

  function toolActivity(raw) {
    let data;
    try {
      data = typeof raw === 'string' ? JSON.parse(raw) : raw;
    } catch (e) {
      return;
    }
    const action = data.action;
    if (action === 'clear') {
      clearToolActivity();
      initScrollBottomFab();
      scheduleScrollChromeUpdate();
      return;
    }
    if (action === 'executing') {
      const host = ensureToolActivityHost();
      const card = document.createElement('div');
      const toolName = data.name || 'tool';
      card.className = 'chat-tool-card chat-tool-card--running chat-tool-card--collapsed';
      card.dataset.toolName = toolName;

      const toggle = document.createElement('button');
      toggle.type = 'button';
      toggle.className = 'chat-tool-card-toggle';

      const chev = document.createElement('span');
      chev.className = 'chat-tool-card-chevron';
      chev.textContent = '▶';
      chev.setAttribute('aria-hidden', 'true');

      const status = document.createElement('span');
      status.className = 'chat-tool-card-status';
      const sp = document.createElement('span');
      sp.className = 'chat-tool-spinner';
      status.appendChild(sp);

      const summary = document.createElement('span');
      summary.className = 'chat-tool-card-summary';
      const argsOneLine = String(data.args || '').replace(/\s+/g, ' ').trim();
      const sumLine = buildToolSummaryLine(toolName, argsOneLine);
      summary.textContent = sumLine;
      if (argsOneLine.length > 0) {
        summary.setAttribute('title', toolName + ' · ' + argsOneLine);
      }

      toggle.appendChild(chev);
      toggle.appendChild(status);
      toggle.appendChild(summary);
      card.appendChild(toggle);

      const details = document.createElement('div');
      details.className = 'chat-tool-card-details';

      const argsPre = document.createElement('pre');
      argsPre.className = 'chat-tool-card-body';
      argsPre.textContent = truncateToolText(data.args, TOOL_ARGS_MAX);
      details.appendChild(argsPre);

      card.appendChild(details);
      wireToolCardToggle(card);

      host.appendChild(card);
      scrollPageToBottom();
      return;
    }
    if (action === 'executed') {
      const host = state.toolActivityHost;
      if (!host) {
        return;
      }
      const running = host.querySelectorAll('.chat-tool-card--running');
      const card = running.length ? running[running.length - 1] : null;
      if (!card) {
        return;
      }
      card.classList.remove('chat-tool-card--running');
      card.classList.add('chat-tool-card--done');

      const statusEl = card.querySelector('.chat-tool-card-status');
      if (statusEl) {
        statusEl.className = 'chat-tool-card-status chat-tool-card-check';
        statusEl.innerHTML = '<svg width="14" height="14" viewBox="0 0 16 16" fill="none" xmlns="http://www.w3.org/2000/svg" aria-hidden="true"><path d="M3 8.5L6.5 12L13 5" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/></svg>';
      }

      const toolName = data.name || card.dataset.toolName || 'tool';
      const resultRaw = String(data.result || '');
      const resultOneLine = resultRaw.replace(/\s+/g, ' ').trim();
      const sumLine = buildToolSummaryLine(toolName, resultOneLine);
      setToolCardSummary(card, sumLine, sumLine);

      const details = card.querySelector('.chat-tool-card-details');
      if (!details) {
        scrollPageToBottom();
        return;
      }

      const divider = document.createElement('div');
      divider.className = 'chat-tool-card-divider';
      details.appendChild(divider);

      const lbl = document.createElement('div');
      lbl.className = 'chat-tool-card-label';
      lbl.textContent = '结果';
      details.appendChild(lbl);

      const resPre = document.createElement('pre');
      resPre.className = 'chat-tool-card-body chat-tool-card-result';
      resPre.textContent = truncateToolText(data.result, TOOL_RESULT_MAX);
      details.appendChild(resPre);

      const resultId = String(++nextToolResultId);
      toolResultCache.set(resultId, resultRaw);
      const detail = document.createElement('button');
      detail.type = 'button';
      detail.className = 'chat-tool-card-full-result';
      detail.textContent = '查看全文';
      detail.setAttribute('aria-expanded', 'false');

      const fullResult = document.createElement('pre');
      fullResult.className = 'chat-tool-card-full-result-panel';
      fullResult.hidden = true;
      fullResult.textContent = toolResultCache.get(resultId) || '';

      detail.addEventListener('click', function (e) {
        e.preventDefault();
        const open = !fullResult.hidden;
        fullResult.hidden = open;
        detail.setAttribute('aria-expanded', open ? 'false' : 'true');
        detail.textContent = open ? '查看全文' : '收起全文';
      });
      details.appendChild(detail);
      details.appendChild(fullResult);

      scrollPageToBottom();
    }
  }

  function createPendingElement() {
    const wrapper = document.createElement('div');
    wrapper.className = 'message assistant pending';

    const bubble = document.createElement('div');
    bubble.className = 'bubble';

    const content = document.createElement('div');
    content.className = 'content';

    const dots = document.createElement('div');
    dots.className = 'typing-dots';

    for (let i = 0; i < 3; i++) {
      const dot = document.createElement('span');
      dot.className = 'dot';
      dots.appendChild(dot);
    }

    content.appendChild(dots);
    bubble.appendChild(content);
    wrapper.appendChild(bubble);
    return wrapper;
  }

  function setPending(pending) {
    state.isPending = pending;
    actions.updateRetryButtons(pending);
    if (pending) {
      if (!state.pendingNode) {
        state.pendingNode = createPendingElement();
        insertIntoActiveTurn(state.pendingNode);
      }
      initScrollBottomFab();
      scheduleScrollChromeUpdate();
      return;
    }

    if (state.pendingNode && state.pendingNode.parentNode) {
      state.pendingNode.parentNode.removeChild(state.pendingNode);
    }
    state.pendingNode = null;
    initScrollBottomFab();
    scheduleScrollChromeUpdate();
  }

  function appendMessage(raw) {
    const message = helpers.parseMessage(raw);
    if (message.role === 'assistant' || message.role === 'error') {
      setPending(false);
    }

    const hadPriorMessages = state.messageList.children.length > 0;
    const node = createMessageElement(message);

    if (message.role === 'user') {
      createActiveTurnShell(node);
      setupUserCollapse(node);
      if (hadPriorMessages) {
        scrollMessageToTop(state.currentTurnEl);
      }
      initScrollBottomFab();
      scheduleScrollChromeUpdate();
      return;
    }

    insertIntoActiveTurn(node);
    setupUserCollapse(node);
    initScrollBottomFab();
    scheduleScrollChromeUpdate();
  }

  function updateMessage(raw) {
    const message = helpers.parseMessage(raw);
    const messageId = message.id || '';
    if (!messageId) {
      return;
    }

    pendingMessageUpdates[messageId] = message;
    schedulePendingMessageFlush();
  }

  function flushPendingMessageUpdates() {
    const updates = Object.keys(pendingMessageUpdates).map(function (id) {
      return pendingMessageUpdates[id];
    });
    Object.keys(pendingMessageUpdates).forEach(function (id) {
      delete pendingMessageUpdates[id];
    });
    messageUpdateFrameId = 0;
    messageUpdateTimerId = 0;

    updates.forEach(function (queuedMessage) {
      updateMessageNow(queuedMessage);
    });
  }

  function hasStreamingCodeUpdatePending() {
    return Object.keys(pendingMessageUpdates).some(function (id) {
      const message = pendingMessageUpdates[id];
      if (!message) {
        return false;
      }
      const role = message.role || 'assistant';
      if ((role !== 'assistant' && role !== 'error') || !isMessageStreamingIncomplete(message)) {
        return false;
      }
      return String(message.text || '').indexOf('```') >= 0;
    });
  }

  function schedulePendingMessageFlush() {
    if (messageUpdateFrameId || messageUpdateTimerId) {
      return;
    }

    if (hasStreamingCodeUpdatePending()) {
      messageUpdateTimerId = window.setTimeout(function () {
        flushPendingMessageUpdates();
      }, STREAMING_CODE_UPDATE_INTERVAL_MS);
      return;
    }

    messageUpdateFrameId = requestAnimationFrame(function () {
      flushPendingMessageUpdates();
    });
  }

  function updateMessageNow(message) {
    const node = state.messageList.querySelector('[data-message-id="' + (message.id || '') + '"]');
    if (!node) {
      appendMessage(message);
      return;
    }

    const bubble = node.querySelector('.bubble');
    if (!bubble) {
      appendMessage(message);
      return;
    }

    if (message.role === 'assistant' || message.role === 'error') {
      setPending(false);
    }

    fillMessageElement(node, bubble, message);
    setupUserCollapse(node);
    initScrollBottomFab();
    scheduleScrollChromeUpdate();
  }

  function hydrateMessages(raw) {
    const messages = helpers.parseMessage(raw);
    resetPendingMessageUpdates();
    clearToolResultCache();
    state.messageList.innerHTML = '';
    state.toolActivityHost = null;
    state.pendingNode = null;
    state.currentTurnEl = null;
    state.turnFillEl = null;
    messages.forEach(function (message) {
      const node = createMessageElement(message);
      state.messageList.appendChild(node);
      setupUserCollapse(node);
    });
    initScrollBottomFab();
    scheduleScrollChromeUpdate();
  }

  function clearMessages() {
    resetPendingMessageUpdates();
    clearToolResultCache();
    state.messageList.innerHTML = '';
    state.toolActivityHost = null;
    state.pendingNode = null;
    state.currentTurnEl = null;
    state.turnFillEl = null;
    initScrollBottomFab();
    scheduleScrollChromeUpdate();
  }

  ChatApp.renderer = {
    appendMessage,
    clearMessages,
    hydrateMessages,
    initScrollBottomFab,
    isNearBottom,
    removeMessageById,
    setPending,
    setupUserCollapse,
    toolActivity,
    updateMessage
  };
})(window);
