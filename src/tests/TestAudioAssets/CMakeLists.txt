project(TestAudioAssets)


lite_add_test(${PROJECT_NAME} SOURCES
    tst_audio_assets.h
    tst_decoding.cpp
    tst_resolution.cpp
    test_main.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE ../../app)
target_link_libraries(${PROJECT_NAME} PRIVATE AutomationTestSupport EditorRuntime)
add_dependencies(${PROJECT_NAME} DsEditorLite)
if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_RPATH "$<TARGET_BUNDLE_DIR:DsEditorLite>/Contents/Frameworks")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        LITE_TEST_APPLICATION_PLUGIN_ROOT="$<TARGET_BUNDLE_DIR:DsEditorLite>/Contents/PlugIns")
endif()
lite_register_test(${PROJECT_NAME} CATEGORY workflow)
