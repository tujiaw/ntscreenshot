target_sources(ntscreenshot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/LocalSearchModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/application/SearchIndexService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/application/SearchProviders.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/infrastructure/DirectoryChangeWatcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/infrastructure/BrowserBookmarkReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/infrastructure/SearchIndexStore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/LocalSearchWidget.cpp
)
