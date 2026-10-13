cmake_minimum_required(VERSION 3.19)
project(JigsawFoundry VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)
find_package(OpenCV REQUIRED COMPONENTS core imgproc)
qt_standard_project_setup()
qt_add_executable(JigsawFoundry
    main.cpp
    MainWindow.h MainWindow.cpp
    PuzzleDetector.h PuzzleDetector.cpp
    PuzzleEdgeDetector.h PuzzleEdgeDetector.cpp
    PuzzleEdgeMatcher.h PuzzleEdgeMatcher.cpp
    PuzzleImage.h
)
target_link_libraries(JigsawFoundry PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets ${OpenCV_LIBS})
target_include_directories(JigsawFoundry PRIVATE ${OpenCV_INCLUDE_DIRS})
