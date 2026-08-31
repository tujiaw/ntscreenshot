(function (global) {
  const ChatApp = global.ChatApp || (global.ChatApp = {});

  ChatApp.state = {
    messageList: null,
    bridge: null,
    pendingNode: null,
    currentTurnEl: null,
    turnFillEl: null,
    autoScrollEnabled: false,
    isPending: false,
    toolActivityHost: null,
    scrollBottomFab: null
  };

  ChatApp.constants = {
    AUTO_SCROLL_THRESHOLD: 48,
    USER_COLLAPSE_MAX_HEIGHT: 86
  };

  ChatApp.icons = {
    copy: '<svg width="13" height="13" viewBox="0 0 16 16" fill="none" xmlns="http://www.w3.org/2000/svg"><rect x="5.5" y="5.5" width="8" height="8" rx="1.5" stroke="currentColor" stroke-width="1.5"/><path d="M10 5V4A1.5 1.5 0 0 0 8.5 2.5H4A1.5 1.5 0 0 0 2.5 4V8.5A1.5 1.5 0 0 0 4 10H5" stroke="currentColor" stroke-width="1.5" stroke-linecap="round"/></svg>',
    retry: '<svg width="13" height="13" viewBox="0 0 16 16" fill="none" xmlns="http://www.w3.org/2000/svg"><path d="M13 8A5 5 0 1 1 8 3c1.38 0 2.63.56 3.54 1.46" stroke="currentColor" stroke-width="1.5" stroke-linecap="round"/><path d="M11.5 1.5V5H8" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/></svg>',
    check: '<svg width="13" height="13" viewBox="0 0 16 16" fill="none" xmlns="http://www.w3.org/2000/svg"><path d="M3 8.5L6.5 12L13 5" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/></svg>',
    chevronDown: '<svg width="13" height="13" viewBox="0 0 16 16" fill="none" xmlns="http://www.w3.org/2000/svg"><path d="M4.25 6.25L8 10L11.75 6.25" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/></svg>',
    chevronUp: '<svg width="13" height="13" viewBox="0 0 16 16" fill="none" xmlns="http://www.w3.org/2000/svg"><path d="M4.25 9.75L8 6L11.75 9.75" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/></svg>'
  };

  ChatApp.helpers = {
    parseMessage(raw) {
      return typeof raw === 'string' ? JSON.parse(raw) : raw;
    },

    copyTextToClipboard(text, onSuccess) {
      const ta = document.createElement('textarea');
      ta.value = text;
      ta.style.cssText = 'position:fixed;left:-9999px;top:-9999px;opacity:0;';
      document.body.appendChild(ta);
      ta.focus();
      ta.select();
      let ok = false;
      try { ok = document.execCommand('copy'); } catch (e) {}
      document.body.removeChild(ta);
      if (ok && typeof onSuccess === 'function') {
        onSuccess();
      }
    }
  };
})(window);
