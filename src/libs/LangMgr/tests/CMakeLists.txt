project(LangMgrTest)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

file(GLOB_RECURSE _src main.cpp)

if (WIN32)
    set(PLATFORM_LIB dwmapi.lib)
endif ()

add_executable(${PROJECT_NAME} ${_src})

find_package(QT NAMES Qt6 Qt5 COMPONENTS Core Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Widgets REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        ${PLATFORM_LIB} LangMgr
)