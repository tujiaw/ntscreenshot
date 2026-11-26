# Settings.cpp 只保留对话框装配与数据绑定；各页面的构建逻辑按职责拆成独立
# 编译单元，避免单个文件持续膨胀。它们共享 SettingsInternal.h 里的小工具。
target_sources(ntscreenshot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Settings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SettingsAppearance.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SettingsBehavior.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SettingsHttpServer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SettingsLocalSearch.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SettingsModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Settings.ui
)
