project(TestAutomationProtocol)

lite_add_test(${PROJECT_NAME}
    SOURCES
        test_main.cpp
        tst_automation_protocol.h
        tst_wire.cpp
        tst_registry.cpp
        tst_public_workflows.cpp
        tst_http.cpp
        ../../app/Automation/Mcp/McpHttpServer.cpp
        ../../app/Automation/Mcp/McpRequestDispatcher.cpp
        ../../app/Automation/Native/NativeJsonRpcDispatcher.cpp
        ../../app/Automation/Public/AdmissionController.cpp
        ../../app/Automation/Public/AutomationAccessPolicy.cpp
        ../../app/Automation/Public/AutomationFileGuard.cpp
        ../../app/Automation/Public/PublicAutomationRegistry.cpp
        ../../app/Automation/Public/PublicAutomationRegistryAdvancedApplication.cpp
        ../../app/Automation/Public/PublicAutomationRegistryAdvancedGui.cpp
        ../../app/Modules/FillLyric/Utils/SplitLyric.cpp
        ../../app/Utils/UiLanguageManager.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE ../../app)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network HttpServer)
find_package(re2 CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE
    AutomationTestSupport
    lite::AutomationWire
    lite::ProjectConverters
    Qt6::Core Qt6::Widgets Qt6::Network Qt6::HttpServer
    re2::re2
)
lite_register_test(${PROJECT_NAME} CATEGORY protocol TIMEOUT 90)
