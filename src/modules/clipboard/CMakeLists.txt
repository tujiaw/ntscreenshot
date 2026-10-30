target_sources(ntscreenshot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/AiFillController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AiFillSettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AiFillSettingsDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ClipboardLiteManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ClipboardStore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ClipboardPreviewWindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/FavoriteSyncState.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/FavoriteSyncController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/FavoriteSyncPage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/FavoriteAccountParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/FavoriteAccountFillController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/HistoryWindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/LlmClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/UiaHelper.cpp
)

set(NT_CLIPBOARD_SYNC_URL "https://ntscreenshot.vercel.app" CACHE STRING "Hosted HTTPS endpoint for clipboard favorites sync")
target_compile_definitions(ntscreenshot PRIVATE NT_CLIPBOARD_SYNC_URL="${NT_CLIPBOARD_SYNC_URL}")
if(WIN32)
    target_link_libraries(ntscreenshot PRIVATE advapi32)
endif()
