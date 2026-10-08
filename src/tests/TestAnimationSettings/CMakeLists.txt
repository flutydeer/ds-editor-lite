project(TestAnimationSettings)

lite_add_test(${PROJECT_NAME}
        SOURCES
        main.cpp
        ../../app/UI/Dialogs/Base/DialogTitleBar.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ../../app
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        lite::GUI
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
# A platform plugin that needs no display, since this runs in CI. It was pinned to "windows",
# which does not exist anywhere else, so the test aborted at startup on every other platform
# rather than running. Naming the platform is not enough on Windows, where Qt does not look in the
# Qt it was built against for plugins: without the path the offscreen plugin is not found and the
# test aborts all the same.
set(qt_platform_plugin_dir
        "$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::QOffscreenIntegrationPlugin>")
set_tests_properties(${PROJECT_NAME} PROPERTIES ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORM_PLUGIN_PATH=${qt_platform_plugin_dir}")
