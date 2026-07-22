project(TestOpenGLWidget)

file(GLOB_RECURSE _src *.h *.cpp)

lite_add_test(${PROJECT_NAME} SOURCES ${_src})

target_include_directories(${PROJECT_NAME} PUBLIC .)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets OpenGLWidgets)
find_package(OpenGL)

target_link_libraries(${PROJECT_NAME} PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::OpenGLWidgets
        OpenGL32
)
