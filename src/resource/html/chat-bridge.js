(function (global) {
  const ChatApp = global.ChatApp;
  const state = ChatApp.state;
  const renderer = ChatApp.renderer;

  function bindBridge(channel) {
    state.bridge = channel.objects.chatBridge;
    if (!state.bridge) {
      return;
    }

    state.bridge.hydrateMessages.connect(renderer.hydrateMessages);
    state.bridge.appendMessage.connect(renderer.appendMessage);
    state.bridge.updateMessage.connect(renderer.updateMessage);
    state.bridge.removeMessage.connect(renderer.removeMessageById);
    state.bridge.clearMessages.connect(renderer.clearMessages);
    state.bridge.requestStateChanged.connect(renderer.setPending);
    state.bridge.toolActivity.connect(renderer.toolActivity);
    state.bridge.notifyReady();
  }

  ChatApp.bridge = {
    bindBridge
  };
})(window);
