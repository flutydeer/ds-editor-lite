project(GUI LANGUAGES CXX)

# Generic, app-agnostic GUI toolkit: reusable controls, the theme system, the
# animation framework, base interfaces and rendering utilities. Organized into
# Controls/Theme/Animation/Base/Utils subfolders; consumed as <lite/GUI/.../Xxx.h>.
qm_find_qt(Gui Svg)

# SvsExpression{,Double}SpinBox evaluate math expressions via tinyexpr.
find_package(unofficial-tinyexpr CONFIG REQUIRED)

if(APPLE)
    file(GLOB_RECURSE _src CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.h ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/*.mm)
else()
    file(GLOB_RECURSE _src CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.h ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
endif()

lite_add_library(GUI STATIC QT_AUTOGEN SOURCES ${_src})
add_library(lite::GUI ALIAS GUI)

target_link_libraries(GUI PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Svg
    qtmediate::Widgets
    unofficial::tinyexpr::tinyexpr
    lite::ADT
    lite::Support
    lite::Tasking
)

# WindowFrameUtils_win uses the Windows DWM API for frame effects.
if(WIN32)
    target_link_libraries(GUI PRIVATE dwmapi)
endif()

# Export the shared lite build-include root so linking lite::GUI puts the
# generated <lite/GUI/*.h> forwarding headers on the consumer's path.
target_include_directories(GUI PUBLIC
    $<BUILD_INTERFACE:${LITE_BUILD_INCLUDE_DIR}>
)

# qm_sync_include flattens a directory, so sync each subfolder separately to
# reproduce the <lite/GUI/<Folder>/Xxx.h> layout.
lite_sync_include(GUI DIRECTORY Controls  PREFIX lite/GUI/Controls  FORCE)
lite_sync_include(GUI DIRECTORY Theme      PREFIX lite/GUI/Theme      FORCE)
lite_sync_include(GUI DIRECTORY Animation  PREFIX lite/GUI/Animation  FORCE)
lite_sync_include(GUI DIRECTORY Base       PREFIX lite/GUI/Base       FORCE)
lite_sync_include(GUI DIRECTORY Utils      PREFIX lite/GUI/Utils      FORCE)
