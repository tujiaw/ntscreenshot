include_guard(GLOBAL)

find_package(Qt6 REQUIRED COMPONENTS
    Concurrent
    Core
    Gui
    Widgets
    Network

    Sql
    LinguistTools
)
find_package(OpenCV REQUIRED COMPONENTS
    core
    features2d
    imgcodecs
    imgproc
    objdetect
    photo
)
