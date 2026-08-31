target_sources(ntscreenshot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/resource/ntscreenshot.qrc
    ${CMAKE_CURRENT_SOURCE_DIR}/resource/darkstyle.qrc
)

add_subdirectory(core)
add_subdirectory(shared)
add_subdirectory(modules)
add_subdirectory(app)
add_subdirectory(libs/QHotkey/QHotkey)

if(WIN32)
    target_sources(ntscreenshot PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/resource/ntscreenshot.rc
        ${CMAKE_CURRENT_SOURCE_DIR}/resource/version.rc
    )
endif()
