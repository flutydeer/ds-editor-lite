project(TestThemeColors)

lite_add_test(${PROJECT_NAME}
        SOURCES
        main.cpp
        ../../app/Model/AppOptions/Options/AppearanceOption.cpp
        ../../app/Resources/lite_res.qrc
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ../../app
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
        TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        lite::GUI
)

lite_register_test(${PROJECT_NAME} CATEGORY unit)
