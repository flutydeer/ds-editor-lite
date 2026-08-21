project(TestCascader)

file(GLOB_RECURSE _src *.h *.cpp)

lite_add_test(${PROJECT_NAME} SOURCES ${_src})

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
)

# Interactive demo — do not run under ctest.
