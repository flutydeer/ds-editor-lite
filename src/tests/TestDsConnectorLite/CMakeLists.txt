project(TestDsConnectorLite)

lite_add_test(${PROJECT_NAME} SOURCES main.cpp)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network)
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    lite::AutomationWire
    lite::DsConnectorCore
)
add_dependencies(${PROJECT_NAME} DsConnectorLite)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    TEST_CONNECTOR_EXECUTABLE_PATH="$<TARGET_FILE:DsConnectorLite>")
lite_register_test(${PROJECT_NAME} CATEGORY protocol TIMEOUT 90)
