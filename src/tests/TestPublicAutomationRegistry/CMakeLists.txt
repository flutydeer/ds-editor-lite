project(TestPublicAutomationRegistry)

lite_add_test(${PROJECT_NAME}
    SOURCES
        main.cpp
        ../../app/Automation/Mcp/McpRequestDispatcher.cpp
        ../../app/Automation/Public/AdmissionController.cpp
        ../../app/Automation/Public/AutomationAccessPolicy.cpp
        ../../app/Automation/Public/AutomationFileGuard.cpp
        ../../app/Automation/Public/PublicAutomationRegistry.cpp
        ../../app/Automation/Public/PublicAutomationRegistryAdvancedApplication.cpp
        ../../app/Automation/Public/PublicAutomationRegistryAdvancedGui.cpp
        ../../app/Modules/FillLyric/Utils/SplitLyric.cpp
        ../../app/Modules/FillLyric/Utils/TaggerRuleOrder.cpp
        ../../app/Modules/FillLyric/Utils/TextSplitter.cpp
        ../../app/Modules/FillLyric/Utils/TextTagger.cpp
        ../../app/Utils/UiLanguageManager.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE ../../app)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets)
find_package(re2 CONFIG REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
    AutomationTestSupport
    lite::AutomationWire
    lite::ProjectConverters
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    re2::re2
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
set_tests_properties(${PROJECT_NAME} PROPERTIES TIMEOUT 30)
