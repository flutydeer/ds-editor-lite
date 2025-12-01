project(TestNewStyle)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets)

file(GLOB_RECURSE _src *.h *.cpp)

add_executable(${PROJECT_NAME} ${_src})

# Enable Qt automation (equivalent to qt_add_executable)
set_target_properties(${PROJECT_NAME} PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    AUTOUIC ON
)

target_include_directories(${PROJECT_NAME} PUBLIC .)

target_link_libraries(${PROJECT_NAME} PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
)