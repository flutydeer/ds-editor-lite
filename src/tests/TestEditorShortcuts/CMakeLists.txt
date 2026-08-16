project(TestEditorShortcuts)

lite_add_test(${PROJECT_NAME}
        SOURCES
        main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ../../app
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Test Widgets)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
)

if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Qt${QT_VERSION_MAJOR}::Test>"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )
endif()

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
set(qt_platform_plugin_dir
        "$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::QOffscreenIntegrationPlugin>")
set_tests_properties(${PROJECT_NAME} PROPERTIES ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORM_PLUGIN_PATH=${qt_platform_plugin_dir}")
