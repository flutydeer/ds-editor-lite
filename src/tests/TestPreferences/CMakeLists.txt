project(TestPreferences)

lite_add_test(${PROJECT_NAME} SOURCES
    test_main.cpp
    tst_inference_settings.cpp
    tst_preferences.h
    tst_automation_option.cpp
    tst_ui_language.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE ../../app)
target_link_libraries(${PROJECT_NAME} PRIVATE EditorPreferencesCore)
lite_register_test(${PROJECT_NAME} CATEGORY unit)
