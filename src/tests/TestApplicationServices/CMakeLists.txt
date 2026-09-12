project(TestApplicationServices)


lite_add_test(${PROJECT_NAME} SOURCES
    ApplicationHarness.h
    tst_application_services.h
    AsyncFileDomainSupport.h
    tst_async_files.cpp
    tst_editor.cpp
    tst_host.cpp
    tst_lyric_rules.cpp
    tst_packages.cpp
    tst_playback.cpp
    tst_presets.cpp
    tst_settings.cpp
    test_main.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE ../../app)
target_link_libraries(${PROJECT_NAME} PRIVATE AutomationTestSupport)
lite_register_test(${PROJECT_NAME} CATEGORY workflow)
