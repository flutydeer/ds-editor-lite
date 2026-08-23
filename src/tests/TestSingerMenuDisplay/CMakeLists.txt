project(TestSingerMenuDisplay)

lite_add_test(${PROJECT_NAME}
        SOURCES
        main.cpp
        ../../app/UI/Controls/TwoLevelComboBox.cpp
        ../../app/Utils/UiLanguageManager.cpp
        ../../libs/PackageManager/Models/PackageInfo.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ../../app
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        lite::GUI
        lite::ProjectModel
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
set(qt_platform_plugin_dir
        "$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::QOffscreenIntegrationPlugin>")
set_tests_properties(${PROJECT_NAME} PROPERTIES ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORM_PLUGIN_PATH=${qt_platform_plugin_dir}")
