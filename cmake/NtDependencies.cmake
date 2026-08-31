include_guard(GLOBAL)

find_package(Qt6 REQUIRED COMPONENTS
    Concurrent
    Core
    Gui
    Widgets
    Network
    WebEngineWidgets
    WebChannel
    Sql
)
find_package(OpenCV REQUIRED COMPONENTS
    core
    features2d
    imgcodecs
    imgproc
    objdetect
    photo
)
