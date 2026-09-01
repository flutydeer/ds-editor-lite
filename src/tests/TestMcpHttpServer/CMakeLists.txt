project(TestMcpHttpServer)

lite_add_test(${PROJECT_NAME}
    SOURCES
        main.cpp
        ../../app/Automation/Mcp/McpHttpServer.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE ../../app)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network HttpServer)

target_link_libraries(${PROJECT_NAME} PRIVATE
    lite::AutomationWire
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::HttpServer
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
set_tests_properties(${PROJECT_NAME} PROPERTIES TIMEOUT 30)
