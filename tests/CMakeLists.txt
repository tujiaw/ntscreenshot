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
if(WIN32)
    add_test(NAME conversation_history_contract
        COMMAND ${CMAKE_COMMAND} -E env
            "PATH=$<TARGET_FILE_DIR:Qt6::Core>;$ENV{PATH}"
            $<TARGET_FILE:assistant_history_tests>)
    add_test(NAME ui_theme_contract
        COMMAND ${CMAKE_COMMAND} -E env
            "PATH=$<TARGET_FILE_DIR:Qt6::Core>;$ENV{PATH}"
            $<TARGET_FILE:ui_theme_tests>)
else()
    add_test(NAME conversation_history_contract COMMAND assistant_history_tests)
    add_test(NAME ui_theme_contract COMMAND ui_theme_tests)
endif()
if(WIN32)
    add_test(NAME local_search_store
        COMMAND ${CMAKE_COMMAND} -E env
            "PATH=$<TARGET_FILE_DIR:Qt6::Core>;$ENV{PATH}"
            $<TARGET_FILE:local_search_tests>)
else()
    add_test(NAME local_search_store COMMAND local_search_tests)
endif()

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
