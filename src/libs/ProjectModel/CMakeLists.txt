project(ProjectModel LANGUAGES CXX)

# The editable project model and its inference data.
file(GLOB_RECURSE _src CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

lite_add_library(ProjectModel STATIC QT_AUTOGEN SOURCES ${_src})
add_library(lite::ProjectModel ALIAS ProjectModel)

# AnchorCurve builds interpolators via opendspx.
find_package(opendspx CONFIG REQUIRED)

target_link_libraries(ProjectModel PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    lite::Core
    lite::ADT
    lite::Support
    lite::MusicBase
    opendspx::interpolator
)

target_include_directories(ProjectModel PUBLIC
    $<BUILD_INTERFACE:${LITE_BUILD_INCLUDE_DIR}>
)

# qm_sync_include flattens a directory, so sync each subfolder separately to
# reproduce the <lite/ProjectModel/<Folder>/Xxx.h> layout.
lite_sync_include(ProjectModel DIRECTORY AppModel          PREFIX lite/ProjectModel/AppModel          FORCE)
lite_sync_include(ProjectModel DIRECTORY Voice             PREFIX lite/ProjectModel/Voice             FORCE)
lite_sync_include(ProjectModel DIRECTORY InferenceData     PREFIX lite/ProjectModel/InferenceData     FORCE)
lite_sync_include(ProjectModel DIRECTORY SingingClipSlicer PREFIX lite/ProjectModel/SingingClipSlicer FORCE)
lite_sync_include(ProjectModel DIRECTORY SingingClipSlicer/Models PREFIX lite/ProjectModel/SingingClipSlicer/Models FORCE)
lite_sync_include(ProjectModel DIRECTORY Utils             PREFIX lite/ProjectModel/Utils             FORCE)
lite_sync_include(ProjectModel DIRECTORY Interface         PREFIX lite/ProjectModel/Interface         FORCE)
