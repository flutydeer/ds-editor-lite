project(TestAudioAssets)


lite_add_test(${PROJECT_NAME} SOURCES
    tst_audio_assets.h
    tst_decoding.cpp
    tst_resolution.cpp
    test_main.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE ../../app)
target_link_libraries(${PROJECT_NAME} PRIVATE AutomationTestSupport EditorRuntime)
lite_register_test(${PROJECT_NAME} CATEGORY workflow)
