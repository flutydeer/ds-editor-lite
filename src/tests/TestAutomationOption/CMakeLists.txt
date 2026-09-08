project(TestAutomationOption)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

lite_add_test(${PROJECT_NAME}
    SOURCES
        main.cpp
        ../../app/Model/AppOptions/Options/AutomationOption.cpp
        ../../app/Automation/Mcp/McpClientConfiguration.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ../../app
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
)

lite_register_test(${PROJECT_NAME} CATEGORY unit)
