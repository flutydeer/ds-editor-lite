project(TestAnchoredCurve)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

file(GLOB_RECURSE _src *.h *.cpp)

qt_add_executable(${PROJECT_NAME} ${_src})

target_include_directories(${PROJECT_NAME} PUBLIC .)

find_path(INTERVAL_TREE_INCLUDE_DIRS "interval-tree/interval_tree.hpp" REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC
        "$<BUILD_INTERFACE:${INTERVAL_TREE_INCLUDE_DIRS}>"
)

target_link_libraries(${PROJECT_NAME} PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
)