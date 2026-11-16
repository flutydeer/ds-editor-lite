project(SynthrtEngine LANGUAGES CXX)

find_package(synthrt CONFIG REQUIRED)
find_package(srt-extract CONFIG REQUIRED)
find_package(stdcorelib CONFIG REQUIRED)

file(GLOB_RECURSE _src CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

lite_add_library(SynthrtEngine STATIC QT_AUTOGEN SOURCES ${_src})
add_library(lite::SynthrtEngine ALIAS SynthrtEngine)

target_link_libraries(SynthrtEngine PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    lite::Core
    lite::Support
    lite::ProjectModel
    srt::synthrt
    srt::diffsinger
    stdcorelib::stdcorelib
    PRIVATE srt::extract
)

target_include_directories(SynthrtEngine PUBLIC
    $<BUILD_INTERFACE:${LITE_BUILD_INCLUDE_DIR}>
)

lite_sync_include(SynthrtEngine PREFIX lite/SynthrtEngine FORCE)
