project(TestLyricPreviewWidget)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

file(GLOB_RECURSE _src *.h *.cpp)

add_executable(${PROJECT_NAME} ${_src})

target_link_libraries(${PROJECT_NAME} PUBLIC
    Qt6::Core Qt6::Gui Qt6::Widgets
)

target_include_directories(${PROJECT_NAME} PUBLIC .)