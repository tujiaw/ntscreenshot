function(nt_add_qt_test test_name target_name)
    if(WIN32)
        add_test(NAME ${test_name}
            COMMAND ${CMAKE_COMMAND} -E env
                "PATH=$<TARGET_FILE_DIR:Qt6::Core>;$ENV{PATH}"
                "QT_PLUGIN_PATH=$<TARGET_FILE_DIR:Qt6::Core>/../plugins"
                $<TARGET_FILE:${target_name}>)
    else()
        add_test(NAME ${test_name} COMMAND ${target_name})
    endif()
endfunction()

add_executable(local_search_tests
    local_search/SearchIndexStoreTest.cpp
    ${NT_SOURCE_DIR}/modules/local_search/infrastructure/SearchIndexStore.cpp
    ${NT_SOURCE_DIR}/modules/local_search/infrastructure/BrowserBookmarkReader.cpp
    ${NT_SOURCE_DIR}/core/pinyin/hanzi_to_pinyin.cpp
)
target_include_directories(local_search_tests PRIVATE ${NT_SOURCE_DIR})
target_link_libraries(local_search_tests PRIVATE Qt6::Core Qt6::Sql)
if(MSVC)
    target_compile_options(local_search_tests PRIVATE /utf-8)
endif()

add_executable(assistant_history_tests
    assistant/ConversationHistoryTest.cpp
    ${NT_SOURCE_DIR}/modules/assistant/runtime/agent/ConversationHistory.cpp
)
target_include_directories(assistant_history_tests PRIVATE ${NT_SOURCE_DIR})
target_link_libraries(assistant_history_tests PRIVATE Qt6::Core)
if(MSVC)
    target_compile_options(assistant_history_tests PRIVATE /utf-8)
endif()

add_executable(ui_theme_tests
    theme/ThemeTokensTest.cpp
    ${NT_SOURCE_DIR}/core/theme/ThemeTokens.cpp
)
target_include_directories(ui_theme_tests PRIVATE ${NT_SOURCE_DIR})
target_link_libraries(ui_theme_tests PRIVATE Qt6::Core Qt6::Gui)
if(MSVC)
    target_compile_options(ui_theme_tests PRIVATE /utf-8)
endif()

add_executable(modern_style_tests
    theme/ModernStyleTest.cpp
    ${NT_SOURCE_DIR}/resource/darkstyle.qrc
)
target_link_libraries(modern_style_tests PRIVATE Qt6::Widgets)
if(MSVC)
    target_compile_options(modern_style_tests PRIVATE /utf-8)
endif()
nt_add_qt_test(conversation_history_contract assistant_history_tests)
nt_add_qt_test(ui_theme_contract ui_theme_tests)
nt_add_qt_test(modern_style_contract modern_style_tests)
set_tests_properties(modern_style_contract PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
nt_add_qt_test(local_search_store local_search_tests)

if(WIN32)
    add_test(
        NAME repository_release_gate
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/RepositoryCheckTest.ps1"
            -Checker "${CMAKE_SOURCE_DIR}/scripts/check-repository.ps1"
    )
    add_test(
        NAME release_version_gate
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/ReleaseCheckTest.ps1"
            -Checker "${CMAKE_SOURCE_DIR}/scripts/check-repository.ps1"
            -Root "${CMAKE_SOURCE_DIR}"
    )
endif()
