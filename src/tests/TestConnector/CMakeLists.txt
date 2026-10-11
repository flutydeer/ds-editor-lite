project(TestConnector)

lite_add_test(${PROJECT_NAME} SOURCES tst_connector.cpp)
find_package(Qt6 REQUIRED COMPONENTS Core Network)
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core Qt6::Network
    lite::AutomationWire
    lite::DsConnectorCore
)
add_dependencies(${PROJECT_NAME} DsConnectorLite)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    TEST_CONNECTOR_EXECUTABLE_PATH="$<TARGET_FILE:DsConnectorLite>")
lite_register_test(${PROJECT_NAME} CATEGORY protocol TIMEOUT 90)
