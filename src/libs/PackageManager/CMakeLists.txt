project(PackageManager LANGUAGES CXX)

# Voicebank package management: scans the search paths (supplied by the app)
# via lite::SynthrtEngine, maps the diffsinger catalog into lite PackageInfo /
# SingerInfo, and answers package/singer lookups. App-agnostic: search paths are
# injected and the scan lifecycle is reported via moduleStatusChanged instead of
# writing AppStatus. Consumed as <lite/PackageManager/.../Xxx.h>.
find_package(synthrt CONFIG REQUIRED)
find_package(stdcorelib CONFIG REQUIRED)

file(GLOB_RECURSE _src CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

lite_add_library(PackageManager STATIC QT_AUTOGEN SOURCES ${_src})
add_library(lite::PackageManager ALIAS PackageManager)

target_link_libraries(PackageManager PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    lite::Core
    lite::ADT
    lite::Support
    lite::ProjectModel
    lite::Tasking
    lite::SynthrtEngine
    srt::synthrt
    srt::diffsinger
    stdcorelib::stdcorelib
)

target_include_directories(PackageManager PUBLIC
    $<BUILD_INTERFACE:${LITE_BUILD_INCLUDE_DIR}>
)

# qm_sync_include flattens a directory, so sync each subfolder separately to
# reproduce the <lite/PackageManager/<Folder>/Xxx.h> layout.
lite_sync_include(PackageManager PREFIX lite/PackageManager FORCE)
lite_sync_include(PackageManager DIRECTORY Models PREFIX lite/PackageManager/Models FORCE)
lite_sync_include(PackageManager DIRECTORY Tasks PREFIX lite/PackageManager/Tasks FORCE)
