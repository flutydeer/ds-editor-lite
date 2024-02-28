project(DsEditorLite)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

file(GLOB_RECURSE _src *.h *.cpp)

if(WIN32)
    set(PLATFORM_LIB dwmapi.lib)
endif()

if(APPLE)
    set(PLATFORM_PROPERTY MACOSX_BUNDLE)
endif()

add_executable(${PROJECT_NAME} ${PLATFORM_PROPERTY} ${_src} Resources/lite_res.qrc)

target_include_directories(${PROJECT_NAME} PUBLIC .)

find_package(SndFile CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC
        SndFile::sndfile
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        ${PLATFORM_LIB}
        talcs::Core talcs::Device talcs::Format talcs::Remote talcs::Synthesis talcs::Midi
        opendspx
        IKg2p
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