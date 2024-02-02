project(DsEditorLite)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

file(GLOB_RECURSE _src *.h *.cpp)

if(WIN32)
    set(PLATFORM_LIB dwmapi.lib)
endif()

add_executable(${PROJECT_NAME} ${_src})

target_include_directories(${PROJECT_NAME} PUBLIC .)

find_package(SndFile CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC
        SndFile::sndfile
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        ${PLATFORM_LIB}
        talcs::Core talcs::Device talcs::Format talcs::Remote talcs::Synthesis
)

set_property(TARGET DeployedTargets APPEND PROPERTY TARGETS ${PROJECT_NAME})