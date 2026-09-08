project(TestEditorGlyphAtlas)

lite_add_test(${PROJECT_NAME}
        SOURCES
        main.cpp
        ../../app/UI/Views/Common/EditorGlyphAtlas.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ../../app
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets GuiPrivate)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::GuiPrivate
)

lite_register_test(${PROJECT_NAME} CATEGORY gui GUI)
