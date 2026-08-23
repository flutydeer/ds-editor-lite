project(TestAutomationRuntimeDimensions)

lite_add_test(${PROJECT_NAME}
    SOURCES
        RuntimeDimensionSupport.cpp
        RuntimeDimensionApplicationPlayback.cpp
        RuntimeDimensionEditor.cpp
        RuntimeDimensionSettingsPackages.cpp
        main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE ../../app)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

target_link_libraries(${PROJECT_NAME} PRIVATE
    AutomationTestSupport
    Qt${QT_VERSION_MAJOR}::Core
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
