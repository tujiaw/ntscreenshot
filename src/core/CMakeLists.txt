target_sources(ntscreenshot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/foundation/AsyncRunner.h
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/IToolModule.h
    ${CMAKE_CURRENT_SOURCE_DIR}/imaging/CodeScanner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imaging/ImageEnhance.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imaging/ImageMatcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imaging/ImageUtil.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imaging/SmartMask.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imaging/detector/RectDetector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/network/HttpRequest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/network/PaddleOcrClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pinyin/hanzi_to_pinyin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/Util.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/LogHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/RunGuard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/settings/SettingModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/theme/ThemeManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/theme/ThemeTokens.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/theme/UiStyler.cpp
)

if(WIN32)
    target_sources(ntscreenshot PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/native/WinClipboard.cpp
    )
elseif(UNIX AND NOT APPLE)
    target_sources(ntscreenshot PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/native/UtilX11.cpp
    )
endif()
