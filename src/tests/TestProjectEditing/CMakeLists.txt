project(TestProjectEditing)


lite_add_test(${PROJECT_NAME} SOURCES
    tst_note_transfer.cpp
    tst_piano_roll_commit.cpp
    tst_project.cpp
    tst_project_editing.h
    test_main.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE ../../app)
target_link_libraries(${PROJECT_NAME} PRIVATE AutomationTestSupport)
lite_register_test(${PROJECT_NAME} CATEGORY domain)
