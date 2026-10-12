target_sources(ntscreenshot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/ImageBrowserModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ImageDirectoryModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ImageCvOperations.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ImageMosaic.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ImageDocument.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ImageConversionDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ImageCanvas.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ImageEditorPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ImageBrowserWindow.cpp
)

# Also repair existing build trees whose compiler dependency database was empty:
# the allocation site must rebuild whenever the window's object layout changes.
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/ImageBrowserModule.cpp
    TARGET_DIRECTORY ntscreenshot APPEND PROPERTY
    OBJECT_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/ImageBrowserWindow.h
        ${CMAKE_CURRENT_SOURCE_DIR}/ImageDocument.h)
