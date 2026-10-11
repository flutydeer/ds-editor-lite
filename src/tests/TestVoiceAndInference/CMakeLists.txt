project(TestVoiceAndInference)

lite_add_test(${PROJECT_NAME} SOURCES
    test_main.cpp
    tst_voice_and_inference.h
    tst_speaker_mix.cpp
    tst_voice_context.cpp
    tst_input_conversion.cpp
    tst_speaker_mix_validation.cpp
    tst_singer_session_cache.cpp
    tst_infer_cache.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE ../../app)
target_link_libraries(${PROJECT_NAME} PRIVATE EditorAutomationCore EditorInferenceCore lite::ProjectModel)
lite_register_test(${PROJECT_NAME} CATEGORY unit)
