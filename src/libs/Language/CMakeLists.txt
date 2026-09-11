project(Language LANGUAGES CXX)

# G2P (grapheme-to-phoneme) language support.
#
# What used to be here -- a convert runner and an input adapter -- wrapped the older line's
# LanguageService and its per-lyric route. The main line has neither: a conversion is asked of a
# (singer, language) pair and wolf's session does the routing, so there is nothing left to adapt
# and lite::SynthrtEngine answers it directly. Only the identifier remains.
# Pure domain (no Qt Widgets); the language-settings widgets live in the app.
# Consumed as <lite/Language/.../Xxx.h>.
find_package(synthrt CONFIG REQUIRED)
find_package(wolf CONFIG REQUIRED)

file(GLOB_RECURSE _src CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

lite_add_library(Language STATIC QT_AUTOGEN SOURCES ${_src})
add_library(lite::Language ALIAS Language)

target_link_libraries(Language PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    lite::Support
    lite::ProjectModel
    synthrt::synthrt
    wolf::wolf
)

target_include_directories(Language PUBLIC
    $<BUILD_INTERFACE:${LITE_BUILD_INCLUDE_DIR}>
)

# qm_sync_include flattens a directory, so sync each subfolder separately to
# reproduce the <lite/Language/<Folder>/Xxx.h> layout.
lite_sync_include(Language PREFIX lite/Language FORCE)
lite_sync_include(Language DIRECTORY Models PREFIX lite/Language/Models FORCE)
