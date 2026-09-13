target_sources(ntscreenshot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/BrowserPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/tools/BackgroundBrowser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AssistantModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/agent/Agent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/agent/AgentToolRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/agent/ConversationHistory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/agent/OpenAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/tools/LlmTool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/tools/LlmToolUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/tools/ToolAbort.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/ChatInputWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/ChatSessionStore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/ChatWebBridge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/ChatWidget.cpp
)
