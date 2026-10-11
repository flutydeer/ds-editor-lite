project(TestGuiComponents)
find_package(Qt6 REQUIRED COMPONENTS GuiPrivate)

lite_add_test(${PROJECT_NAME} SOURCES
    test_main.cpp tst_gui_components.h tst_theme.cpp tst_two_level_combo_box.cpp tst_animation.cpp
    tst_input_controls.cpp
    tst_path_controls.cpp
    tst_reorder_handles.cpp tst_pointer_tooltips.cpp tst_touch_claims.cpp
)
target_compile_definitions(${PROJECT_NAME} PRIVATE TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(${PROJECT_NAME} PRIVATE EditorGuiCore Qt6::GuiPrivate)
lite_register_test(${PROJECT_NAME} CATEGORY gui GUI)
