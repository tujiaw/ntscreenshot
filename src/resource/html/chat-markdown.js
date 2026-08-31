(function (global) {
  const ChatApp = global.ChatApp;
  const marked = global.marked;

  function escapeHtml(s) {
    return String(s)
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;');
  }

  function sanitizeHtml(html) {
    const template = document.createElement('template');
    template.innerHTML = String(html || '');

    const blockedTags = {
      EMBED: true,
      FORM: true,
      IFRAME: true,
      INPUT: true,
      LINK: true,
      META: true,
      OBJECT: true,
      SCRIPT: true,
      STYLE: true,
      TEXTAREA: true
    };

    const walker = document.createTreeWalker(template.content, NodeFilter.SHOW_ELEMENT, null);
    const elements = [];
    while (walker.nextNode()) {
      elements.push(walker.currentNode);
    }

    elements.forEach(function (el) {
      const tagName = el.tagName;
      if (blockedTags[tagName]) {
        el.remove();
        return;
      }

      const attrs = Array.prototype.slice.call(el.attributes || []);
      attrs.forEach(function (attr) {
        const name = attr.name.toLowerCase();
        const value = attr.value || '';
        if (name.indexOf('on') === 0 || name === 'srcdoc') {
          el.removeAttribute(attr.name);
          return;
        }
        if ((name === 'href' || name === 'src' || name === 'xlink:href')
          && /^\s*javascript:/i.test(value)) {
          el.removeAttribute(attr.name);
        }
      });

      if (tagName === 'A') {
        const href = el.getAttribute('href') || '';
        if (!/^(https?:|mailto:|#|\/)/i.test(href) && href !== '') {
          el.removeAttribute('href');
        }
      }
    });

    return template.innerHTML;
  }

  function configureMarked() {
    if (!marked || typeof marked.use !== 'function') {
      return;
    }
    marked.use({
      breaks: true,
      gfm: true,
      hooks: {
        postprocess: function (html) {
          return html.replace(
            /<a href=/g,
            '<a target="_blank" rel="noopener noreferrer" href='
          );
        }
      }
    });
  }

  function renderMarkdown(text) {
    const raw = String(text || '').replace(/\r\n/g, '\n');
    if (!marked || typeof marked.parse !== 'function') {
      return '<p>' + escapeHtml(raw) + '</p>';
    }
    try {
      const html = marked.parse(raw, { async: false });
      if (!html || !String(html).trim()) {
        return '<p></p>';
      }
      return sanitizeHtml(html);
    } catch (e) {
      return '<p>' + escapeHtml(raw) + '</p>';
    }
  }

  configureMarked();

  ChatApp.markdown = {
    renderMarkdown: renderMarkdown
  };
})(window);
