project(TestAnchoredCurve)

file(GLOB_RECURSE _src *.h *.cpp)

lite_add_test(${PROJECT_NAME} SOURCES ${_src})

target_include_directories(${PROJECT_NAME} PUBLIC .)

find_path(INTERVAL_TREE_INCLUDE_DIRS "interval-tree/interval_tree.hpp" REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC
        "$<BUILD_INTERFACE:${INTERVAL_TREE_INCLUDE_DIRS}>"
)

target_link_libraries(${PROJECT_NAME} PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
)
