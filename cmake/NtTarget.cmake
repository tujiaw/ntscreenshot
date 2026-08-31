include_guard(GLOBAL)

function(nt_configure_target target_name)
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR "nt_configure_target: unknown target '${target_name}'")
    endif()

    target_include_directories(${target_name} PRIVATE
        ${NT_SOURCE_DIR}
        ${NT_THIRD_PARTY_DIR}/QHotkey/QHotkey
    )

    target_compile_definitions(${target_name} PRIVATE
        NT_SKILLS_SOURCE_DIR="${NT_SOURCE_DIR}/resource/skills"
        NT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        NT_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        NT_VERSION_PATCH=${PROJECT_VERSION_PATCH}
        NT_VERSION_STRING="${PROJECT_VERSION}.0"
    )

    target_link_libraries(${target_name} PRIVATE
        Qt6::Concurrent
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        Qt6::WebEngineWidgets
        Qt6::WebChannel
        Qt6::Sql
    )

    if(TARGET opencv_core)
        target_link_libraries(${target_name} PRIVATE
            opencv_core
            opencv_imgproc
            opencv_imgcodecs
            opencv_features2d
            opencv_objdetect
            opencv_photo
        )
    else()
        target_link_libraries(${target_name} PRIVATE ${OpenCV_LIBS})
        target_include_directories(${target_name} PRIVATE ${OpenCV_INCLUDE_DIRS})
    endif()

    if(WIN32)
        set_target_properties(${target_name} PROPERTIES WIN32_EXECUTABLE TRUE)
        target_link_libraries(${target_name} PRIVATE
            shell32
            user32
            gdi32
            ole32
            oleaut32
            comsuppw
            uuid
            UIAutomationCore
        )
    elseif(UNIX AND NOT APPLE)
        find_package(X11 REQUIRED)
        find_library(NT_XTST_LIBRARY Xtst REQUIRED)
        find_library(NT_XI_LIBRARY Xi REQUIRED)
        find_library(NT_XKBFILE_LIBRARY xkbfile REQUIRED)
        target_link_libraries(${target_name} PRIVATE
            X11::X11
            xcb
            ${NT_XTST_LIBRARY}
            ${NT_XI_LIBRARY}
            ${NT_XKBFILE_LIBRARY}
        )
        target_compile_definitions(${target_name} PRIVATE NT_HAVE_X11=1)
    elseif(APPLE)
        find_library(NT_CARBON_LIBRARY Carbon REQUIRED)
        target_link_libraries(${target_name} PRIVATE ${NT_CARBON_LIBRARY})
    endif()

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /utf-8)
    endif()

    set_property(TARGET ${target_name} PROPERTY
        AUTOUIC_SEARCH_PATHS "${NT_SOURCE_DIR}/modules/settings"
    )

    get_target_property(_nt_target_sources ${target_name} SOURCES)
    source_group(TREE "${NT_SOURCE_DIR}" PREFIX "Source Files" FILES ${_nt_target_sources})
endfunction()
