project(TestThemeIcons)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_executable(${PROJECT_NAME}
        main.cpp
        Stubs.cpp
        ../../app/UI/Utils/AppColorPalette.cpp
        ../../app/UI/Utils/ColorUtils.cpp
        ../../app/UI/Utils/IconUtils.cpp
        ../../app/UI/Utils/ThemeManager.cpp
        ../../app/UI/Utils/Theme/ThemeColorResolver.cpp
        ../../app/UI/Utils/Theme/ThemeLoader.cpp
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
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
set(qt_platform_plugin_dir
        "$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::QOffscreenIntegrationPlugin>")
set_tests_properties(${PROJECT_NAME} PROPERTIES ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORM_PLUGIN_PATH=${qt_platform_plugin_dir}")
