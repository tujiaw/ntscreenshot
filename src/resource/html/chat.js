(function (global) {
  const ChatApp = global.ChatApp;

  function bootstrap() {
    ChatApp.state.messageList = document.getElementById('message-list');
    if (ChatApp.renderer.initScrollBottomFab) {
      ChatApp.renderer.initScrollBottomFab();
    }
    new QWebChannel(qt.webChannelTransport, ChatApp.bridge.bindBridge);
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', bootstrap);
  } else {
    bootstrap();
  }
})(window);
