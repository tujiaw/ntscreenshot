# Tests load Qt/OpenCV through the PATH configured below. vcpkg's parallel
# app-local copy hooks otherwise race over the same DLLs in this shared output
# directory. Keep this directory-scoped so application deployment is unchanged.
set(VCPKG_APPLOCAL_DEPS OFF)

function(nt_add_qt_test test_name target_name)
    if(WIN32)
        add_test(NAME ${test_name}
            COMMAND ${CMAKE_COMMAND} -E env
                "PATH=$<TARGET_FILE_DIR:Qt6::Core>;$ENV{PATH}"
                "QT_PLUGIN_PATH=$<TARGET_FILE_DIR:Qt6::Core>/../plugins"
                $<TARGET_FILE:${target_name}> ${ARGN})
    else()
        add_test(NAME ${test_name} COMMAND ${target_name} ${ARGN})
    endif()
endfunction()

target_sources(ntscreenshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/capture/ScreenshotSelectionTest.cpp)
target_compile_definitions(ntscreenshot PRIVATE NT_BUILD_SELECTION_TEST=1)

# Imported targets expose the actual DLL directory for both prebuilt OpenCV
# packages and locally built minimal runtimes.
set(nt_opencv_runtime_dirs "")
foreach(opencv_library IN LISTS OpenCV_LIBS)
    if(TARGET "${opencv_library}")
        list(APPEND nt_opencv_runtime_dirs "$<TARGET_FILE_DIR:${opencv_library}>")
    endif()
endforeach()
list(REMOVE_DUPLICATES nt_opencv_runtime_dirs)
add_test(NAME screenshot_selection_release COMMAND ${CMAKE_COMMAND} -E env
    "NTSCREENSHOT_SELECTION_TEST=1"
    "QT_FORCE_STDERR_LOGGING=1"
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>;${nt_opencv_runtime_dirs};$ENV{PATH}"
    "QT_PLUGIN_PATH=$<TARGET_FILE_DIR:Qt6::Core>/../plugins"
    $<TARGET_FILE:ntscreenshot>)
set_tests_properties(screenshot_selection_release PROPERTIES TIMEOUT 30)

add_executable(translation_tests i18n/TranslationTest.cpp)
target_link_libraries(translation_tests PRIVATE Qt6::Core)
if(MSVC)
    target_compile_options(translation_tests PRIVATE /utf-8)
endif()

nt_add_qt_test(english_translation translation_tests
    "${CMAKE_CURRENT_BINARY_DIR}/../ntscreenshot_en.qm")

add_executable(draw_settings_layout_tests
    capture/DrawSettingsLayoutTest.cpp
    ${NT_SOURCE_DIR}/modules/capture/annotation/DrawSettings.cpp)
target_include_directories(draw_settings_layout_tests PRIVATE ${NT_SOURCE_DIR})
target_compile_definitions(draw_settings_layout_tests PRIVATE
    DRAW_SETTINGS_QSS_PATH="${NT_SOURCE_DIR}/resource/darkstyle/modernstyle.qss")
target_link_libraries(draw_settings_layout_tests PRIVATE Qt6::Widgets)
if(MSVC)
    target_compile_options(draw_settings_layout_tests PRIVATE /utf-8)
endif()
nt_add_qt_test(draw_settings_layout draw_settings_layout_tests)
set_tests_properties(draw_settings_layout PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(image_util_auto_crop_tests
    imaging/ImageUtilAutoCropTest.cpp
    ${NT_SOURCE_DIR}/core/imaging/ImageUtil.cpp)
target_include_directories(image_util_auto_crop_tests PRIVATE ${NT_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
target_link_libraries(image_util_auto_crop_tests PRIVATE Qt6::Core Qt6::Gui ${OpenCV_LIBS})
if(MSVC)
    target_compile_options(image_util_auto_crop_tests PRIVATE /utf-8)
endif()
if(WIN32)
    add_test(NAME image_util_auto_crop
        COMMAND ${CMAKE_COMMAND} -E env
            "PATH=$<TARGET_FILE_DIR:Qt6::Core>;${nt_opencv_runtime_dirs};$ENV{PATH}"
            $<TARGET_FILE:image_util_auto_crop_tests>)
else()
    add_test(NAME image_util_auto_crop COMMAND image_util_auto_crop_tests)
endif()

add_executable(image_matcher_tests
    capture/ImageMatcherTest.cpp
    ${NT_SOURCE_DIR}/core/imaging/ImageMatcher.cpp)
target_include_directories(image_matcher_tests PRIVATE ${NT_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
target_link_libraries(image_matcher_tests PRIVATE Qt6::Core Qt6::Gui ${OpenCV_LIBS})
if(MSVC)
    target_compile_options(image_matcher_tests PRIVATE /utf-8)
endif()
if(WIN32)
    add_test(NAME long_screenshot_matcher
        COMMAND ${CMAKE_COMMAND} -E env
            "PATH=$<TARGET_FILE_DIR:Qt6::Core>;${nt_opencv_runtime_dirs};$ENV{PATH}"
            $<TARGET_FILE:image_matcher_tests>)
else()
    add_test(NAME long_screenshot_matcher COMMAND image_matcher_tests)
endif()

add_executable(code_scanner_tests
    imaging/CodeScannerTest.cpp
    ${NT_SOURCE_DIR}/core/imaging/CodeScanner.cpp)
target_include_directories(code_scanner_tests PRIVATE ${NT_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
target_link_libraries(code_scanner_tests PRIVATE Qt6::Core Qt6::Gui ${OpenCV_LIBS})
if(MSVC)
    target_compile_options(code_scanner_tests PRIVATE /utf-8)
endif()
if(WIN32)
    add_test(NAME qr_code_decode
        COMMAND ${CMAKE_COMMAND} -E env
            "PATH=$<TARGET_FILE_DIR:Qt6::Core>;${nt_opencv_runtime_dirs};$ENV{PATH}"
            $<TARGET_FILE:code_scanner_tests>)
else()
    add_test(NAME qr_code_decode COMMAND code_scanner_tests)
endif()

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
    ${NT_SOURCE_DIR}/core/theme/ModernStyle.cpp
    ${NT_SOURCE_DIR}/resource/darkstyle.qrc
)
target_include_directories(modern_style_tests PRIVATE ${NT_SOURCE_DIR})
target_link_libraries(modern_style_tests PRIVATE Qt6::Widgets)
if(MSVC)
    target_compile_options(modern_style_tests PRIVATE /utf-8)
endif()

add_executable(http_server_tests
    app/HttpServerControllerTest.cpp
    ${NT_SOURCE_DIR}/app/HttpServerController.cpp
)
target_include_directories(http_server_tests PRIVATE ${NT_SOURCE_DIR})
target_link_libraries(http_server_tests PRIVATE Qt6::Core Qt6::Network)
if(MSVC)
    target_compile_options(http_server_tests PRIVATE /utf-8)
endif()

nt_add_qt_test(ui_theme_contract ui_theme_tests)
nt_add_qt_test(modern_style_contract modern_style_tests)
set_tests_properties(modern_style_contract PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
nt_add_qt_test(local_search_store local_search_tests)
nt_add_qt_test(http_server_arguments http_server_tests)
if(WIN32)
 add_test(NAME repository_release_gate COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/RepositoryCheckTest.ps1" -Checker "${CMAKE_SOURCE_DIR}/scripts/check-repository.ps1")
 add_test(NAME release_version_gate COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/ReleaseCheckTest.ps1" -Checker "${CMAKE_SOURCE_DIR}/scripts/check-repository.ps1" -Root "${CMAKE_SOURCE_DIR}")
endif()

if(WIN32)
    add_executable(clipboard_history_tests clipboard/ClipboardHistoryTest.cpp
        ${NT_SOURCE_DIR}/modules/clipboard/ClipboardStore.cpp)
    target_include_directories(clipboard_history_tests PRIVATE ${NT_SOURCE_DIR})
    target_link_libraries(clipboard_history_tests PRIVATE Qt6::Core Qt6::Sql)
    if(MSVC)
        target_compile_options(clipboard_history_tests PRIVATE /utf-8)
    endif()
    nt_add_qt_test(clipboard_history_contract clipboard_history_tests)
endif()
nt_add_qt_test(http_server_startup http_server_tests --integration)
set_tests_properties(http_server_startup PROPERTIES SKIP_RETURN_CODE 77)
