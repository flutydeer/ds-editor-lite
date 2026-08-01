project(LitePalette VERSION ${LITE_VERSION})

set(_src
    main.cpp
)

lite_add_tool(${PROJECT_NAME}
    SOURCES ${_src}
    INCLUDE_PRIVATE .
    FEATURES_PRIVATE cxx_std_20
    QT_LINKS_PRIVATE Core Gui
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    lite::GUI
)
