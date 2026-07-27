project(Language LANGUAGES CXX)

# G2P (grapheme-to-phoneme) language support: the convert runner, the input
# adapter and the singer-G2P identifier. Wraps the synthrt G2P services.
# Pure domain (no Qt Widgets); the language-settings widgets live in the app.
# Consumed as <lite/Language/.../Xxx.h>.
find_package(synthrt CONFIG REQUIRED)

file(GLOB_RECURSE _src CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

lite_add_library(Language STATIC QT_AUTOGEN SOURCES ${_src})
add_library(lite::Language ALIAS Language)

target_link_libraries(Language PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    lite::Support
    lite::ProjectModel
    srt::synthrt
)

target_include_directories(Language PUBLIC
    $<BUILD_INTERFACE:${LITE_BUILD_INCLUDE_DIR}>
)

# qm_sync_include flattens a directory, so sync each subfolder separately to
# reproduce the <lite/Language/<Folder>/Xxx.h> layout.
lite_sync_include(Language PREFIX lite/Language FORCE)
lite_sync_include(Language DIRECTORY Models PREFIX lite/Language/Models FORCE)
