project(TestAutomationApplicationDomains)

lite_add_test(${PROJECT_NAME}
    SOURCES
        main.cpp
        TestAutomationApplicationDomains.h
        ApplicationHarness.h
        Host.cpp
        Playback.cpp
        Editor.cpp
        Settings.cpp
        LyricRules.cpp
        Packages.cpp
        Presets.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE AutomationTestSupport Qt6::Core)
lite_register_test(${PROJECT_NAME} CATEGORY domain)
