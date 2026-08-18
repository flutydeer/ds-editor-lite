project(TestTwoLevelComboBox)

lite_add_test(${PROJECT_NAME}
        SOURCES
        main.cpp
        ../../app/UI/Controls/TwoLevelComboBox.cpp
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
        lite::ProjectModel
        lite::PackageManager
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
set(qt_platform_plugin_dir
        "$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::QOffscreenIntegrationPlugin>")
set_tests_properties(${PROJECT_NAME} PROPERTIES ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORM_PLUGIN_PATH=${qt_platform_plugin_dir}")
