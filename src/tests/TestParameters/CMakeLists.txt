project(TestParameters)

lite_add_test(${PROJECT_NAME} SOURCES
    test_main.cpp
    tst_parameters.h
    tst_param_resample.cpp
    tst_param_support.cpp
    tst_curve_trace.cpp
    tst_curve_transform.cpp
    tst_anchor_edit_controller.cpp
    tst_pitch_display_strategy.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE ../../app)
target_link_libraries(${PROJECT_NAME} PRIVATE EditorAutomationCore EditorInferenceCore Qt6::Gui)
lite_register_test(${PROJECT_NAME} CATEGORY domain)
