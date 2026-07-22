project(LitePalette VERSION ${LITE_VERSION})

file(GLOB_RECURSE _src CONFIGURE_DEPENDS *.h *.cpp)

lite_add_tool(${PROJECT_NAME}
    SOURCES ${_src}
    INCLUDE_PRIVATE .
    FEATURES_PRIVATE cxx_std_20
    QT_LINKS_PRIVATE Core Widgets
)
