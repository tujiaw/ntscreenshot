target_sources(ntscreenshot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/CaptureModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/annotation/DrawPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/annotation/DrawSettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gif_recorder/GifEncoderWorker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gif_recorder/GifRecorderControlPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gif_recorder/GifRecorderWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/long_screenshot/LongScreenshotControlPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/long_screenshot/LongScreenshotWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pin/PinWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/screenshot/Amplifier.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/screenshot/Screenshot.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/screenshot/ScreenshotActionController.cpp
)
