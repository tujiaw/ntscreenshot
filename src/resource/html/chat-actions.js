(function (global) {
  const ChatApp = global.ChatApp;
  const state = ChatApp.state;
  const icons = ChatApp.icons;
  const helpers = ChatApp.helpers;

  function makeCopyButton(getText) {
    const btn = document.createElement('button');
    btn.className = 'msg-action-btn msg-copy-btn';
    btn.innerHTML = icons.copy;
    btn.title = '复制';
    btn.addEventListener('click', function (e) {
      e.stopPropagation();
      helpers.copyTextToClipboard(getText(), function () {
        btn.innerHTML = icons.check;
        btn.classList.add('success');
        setTimeout(function () {
          btn.innerHTML = icons.copy;
          btn.classList.remove('success');
        }, 1500);
      });
    });
    return btn;
  }

  function makeCodeCopyButton() {
    const btn = document.createElement('button');
    btn.className = 'msg-action-btn code-copy-btn';
    btn.type = 'button';
    btn.innerHTML = icons.copy;
    btn.title = '复制代码';
    btn.addEventListener('click', function (e) {
      e.stopPropagation();
      const pre = btn.parentElement;
      const code = pre ? pre.querySelector('code') : null;
      const text = code ? code.textContent || '' : (pre ? pre.textContent || '' : '');
      helpers.copyTextToClipboard(text, function () {
        btn.innerHTML = icons.check;
        btn.classList.add('success');
        setTimeout(function () {
          btn.innerHTML = icons.copy;
          btn.classList.remove('success');
        }, 1500);
      });
    });
    return btn;
  }

  function makeRetryButton() {
    const btn = document.createElement('button');
    btn.className = 'msg-action-btn msg-retry-btn';
    btn.innerHTML = icons.retry;
    btn.title = '重新生成';
    if (state.isPending) {
      btn.disabled = true;
    }
    btn.addEventListener('click', function (e) {
      e.stopPropagation();
      if (state.bridge && !state.isPending) {
        state.bridge.requestRetry();
      }
    });
    return btn;
  }

  function makeToggleButton(expanded) {
    const btn = document.createElement('button');
    btn.className = 'msg-action-btn bubble-toggle-btn';
    btn.type = 'button';
    syncToggleButton(btn, expanded);
    return btn;
  }

  function syncToggleButton(button, expanded) {
    button.setAttribute('aria-expanded', expanded ? 'true' : 'false');
    button.title = expanded ? '收起' : '展开';
    button.innerHTML = expanded ? icons.chevronUp : icons.chevronDown;
  }

  function updateRetryButtons(pending) {
    const buttons = document.querySelectorAll('.msg-retry-btn');
    buttons.forEach(function (btn) {
      btn.disabled = pending;
    });
  }

  ChatApp.actions = {
    makeCopyButton,
    makeCodeCopyButton,
    makeRetryButton,
    makeToggleButton,
    syncToggleButton,
    updateRetryButtons
  };
})(window);
