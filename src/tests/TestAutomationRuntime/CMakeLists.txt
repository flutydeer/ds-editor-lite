project(TestAutomationRuntime)

find_package(Threads REQUIRED)

lite_add_test(${PROJECT_NAME} SOURCES
    tst_admission.cpp
    tst_automation_runtime.h
    ControlledRaceSupport.h
    tst_dispatch.cpp
    tst_file_access.cpp
    tst_idempotency.cpp
    tst_tasks.cpp
    test_main.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE ../../app)
target_link_libraries(${PROJECT_NAME} PRIVATE AutomationTestSupport Threads::Threads)
lite_register_test(${PROJECT_NAME} CATEGORY domain)
