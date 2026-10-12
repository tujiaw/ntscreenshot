# This dependency crosses the allocation/constructor boundary. Losing it can
# produce a successful link followed by a heap overflow on opening the browser.
foreach(source IN ITEMS
    src/modules/image_browser/ImageBrowserModule.cpp
    src/modules/image_browser/ImageBrowserWindow.cpp
    src/core/platform/SystemBackground.cpp)
    execute_process(
        COMMAND "${NT_NINJA}" -t deps
            "CMakeFiles/ntscreenshot.dir/${source}.obj"
        WORKING_DIRECTORY "${NT_BUILD_DIR}"
        OUTPUT_VARIABLE dependencies
        ERROR_VARIABLE dependency_error
        RESULT_VARIABLE dependency_result
        ENCODING UTF-8)
    if(NOT dependency_result EQUAL 0 OR NOT dependencies MATCHES "(ImageBrowserWindow|SystemBackground)[.]h")
        message(FATAL_ERROR "MSVC header dependencies are missing for ${source}; incremental builds are unsafe.\n${dependency_error}\n${dependencies}")
    endif()
endforeach()
