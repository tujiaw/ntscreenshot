target_sources(ntscreenshot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/qhotkey.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/qhotkey.h
    ${CMAKE_CURRENT_SOURCE_DIR}/qhotkey_p.h
)

if(WIN32)
    target_sources(ntscreenshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/qhotkey_win.cpp)
elseif(UNIX AND NOT APPLE)
    target_sources(ntscreenshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/qhotkey_x11.cpp)
elseif(APPLE)
    target_sources(ntscreenshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/qhotkey_mac.cpp)
endif()
