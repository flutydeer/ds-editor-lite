project(TestNativeDesktop)

lite_add_test(${PROJECT_NAME} SOURCES
    test_main.cpp tst_native_desktop.h tst_splitter.cpp tst_animation_settings.cpp tst_rhi_editor.cpp
)
target_link_libraries(${PROJECT_NAME} PRIVATE EditorRuntime)
add_dependencies(${PROJECT_NAME} DsEditorLite)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    LITE_TEST_APPLICATION_CONFIG_ROOT="$<TARGET_FILE_DIR:DsEditorLite>/configs")
if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_RPATH "$<TARGET_BUNDLE_DIR:DsEditorLite>/Contents/Frameworks")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        LITE_TEST_APPLICATION_PLUGIN_ROOT="$<TARGET_BUNDLE_DIR:DsEditorLite>/Contents/PlugIns")
endif()
lite_register_test(${PROJECT_NAME} CATEGORY gui NATIVE_GUI)
