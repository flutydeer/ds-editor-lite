project(DsEditorLite)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(APPLE)
    file(GLOB_RECURSE _src *.h *.cpp *.mm)
else()
    file(GLOB_RECURSE _src *.h *.cpp)
endif()

if(WIN32)
    set(PLATFORM_LIB dwmapi.lib)
endif()

if(APPLE)
    set(PLATFORM_PROPERTY MACOSX_BUNDLE)
endif()

add_executable(${PROJECT_NAME} ${PLATFORM_PROPERTY} ${_src} Resources/lite_res.qrc)

target_include_directories(${PROJECT_NAME} PUBLIC .)

find_package(SndFile CONFIG REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets OpenGLWidgets)
find_package(OpenGL)
target_link_libraries(${PROJECT_NAME} PUBLIC
        SndFile::sndfile
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::OpenGLWidgets
        OpenGL32
        ${PLATFORM_LIB}
        talcs::Core talcs::Device talcs::Format talcs::Remote talcs::Midi talcs::Dspx
        opendspx
        LangMgr
)

set_property(TARGET DeployedTargets APPEND PROPERTY TARGETS ${PROJECT_NAME})

# copy phoneme dict
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND
        ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/Resources
        ${CMAKE_BINARY_DIR}/bin/Resources
)
install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Resources
        DESTINATION .
)