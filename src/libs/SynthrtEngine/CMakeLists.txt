project(SynthrtEngine LANGUAGES CXX)

# Facade over the synthrt v2 components (voicebank scanner, language service,
# runtime + ONNX driver, singer stage resolver, model set). Owns singer model
# sessions and the package catalog. App-agnostic; consumed as
# <lite/SynthrtEngine/Xxx.h>. Its singleton is registered by the app's
# AppContext through the SingletonRegistry.
find_package(synthrt CONFIG REQUIRED)
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
)

target_include_directories(SynthrtEngine PUBLIC
    $<BUILD_INTERFACE:${LITE_BUILD_INCLUDE_DIR}>
)

lite_sync_include(SynthrtEngine PREFIX lite/SynthrtEngine FORCE)
