target_sources(ntscreenshot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ModuleRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MyApplication.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WindowManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/crash/MiniDump.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shell/GlobalHotkeyRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shell/MainWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shell/ShellModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shell/SystemTray.cpp
)
