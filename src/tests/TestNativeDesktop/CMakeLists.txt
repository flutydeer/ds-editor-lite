project(TestNativeDesktop)

lite_add_test(${PROJECT_NAME} SOURCES
    test_main.cpp tst_native_desktop.h tst_splitter.cpp tst_animation_settings.cpp
)
target_link_libraries(${PROJECT_NAME} PRIVATE EditorGuiCore)
lite_register_test(${PROJECT_NAME} CATEGORY gui NATIVE_GUI)
