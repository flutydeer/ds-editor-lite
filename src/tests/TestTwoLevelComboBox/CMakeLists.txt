project(TestTwoLevelComboBox)

lite_add_test(${PROJECT_NAME}
        SOURCES
        main.cpp
        ../../app/UI/Controls/TwoLevelComboBox.cpp
        ../../app/Utils/UiLanguageManager.cpp
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

lite_register_test(${PROJECT_NAME} CATEGORY gui GUI)
