project(TestThemeIcons)

lite_add_test(${PROJECT_NAME}
        SOURCES
        main.cpp
        Stubs.cpp
        ../../app/UI/Utils/AppColorPalette.cpp
        ../../app/Model/AppOptions/Options/AppearanceOption.cpp
        ../../app/Resources/lite_res.qrc
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ../../app
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets Svg)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Svg
        lite::GUI
)

lite_register_test(${PROJECT_NAME} CATEGORY gui GUI)
