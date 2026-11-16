project(ProjectConverters LANGUAGES CXX)

# Project-file import and export. The application handles interactive choices.
find_package(opendspx CONFIG REQUIRED)

if(APPLE)
    file(GLOB_RECURSE _src CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.h ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/*.mm)
else()
    file(GLOB_RECURSE _src CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.h ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
endif()

lite_add_library(ProjectConverters STATIC QT_AUTOGEN SOURCES ${_src})
add_library(lite::ProjectConverters ALIAS ProjectConverters)

target_link_libraries(ProjectConverters PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    lite::ProjectModel
    lite::PackageManager
    opendspx::model
    opendspx::serializer
    opendspx::converter
)

# ICU backs MidiTextCodecConverter on Windows / Linux (macOS uses the .mm).
if(WIN32)
    target_link_libraries(ProjectConverters PRIVATE icu)
elseif(NOT APPLE)
    # Charset detection requires icui18n as well as icuuc on Linux.
    find_package(ICU COMPONENTS uc i18n REQUIRED)
    target_link_libraries(ProjectConverters PRIVATE ICU::uc ICU::i18n)
endif()

target_include_directories(ProjectConverters PUBLIC
    $<BUILD_INTERFACE:${LITE_BUILD_INCLUDE_DIR}>
)

lite_sync_include(ProjectConverters PREFIX lite/ProjectConverters FORCE)
