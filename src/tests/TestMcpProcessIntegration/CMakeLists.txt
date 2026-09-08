project(TestMcpProcessIntegration)

lite_add_test(${PROJECT_NAME} SOURCES main.cpp ../TestSupport/ProcessFixture.cpp)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network)
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    lite::AutomationWire
    lite::DsConnectorCore
)
add_dependencies(${PROJECT_NAME} DsEditorLite DsConnectorLite)

lite_register_test(${PROJECT_NAME} CATEGORY process GUI TIMEOUT 150
    ARGS
        "$<TARGET_FILE:DsEditorLite>"
        "$<TARGET_FILE:DsConnectorLite>"
)
