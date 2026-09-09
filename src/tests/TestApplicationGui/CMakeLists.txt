project(TestApplicationGui)

lite_add_test(${PROJECT_NAME} SOURCES
    test_main.cpp
    tst_application_gui.h
    tst_piano_roll.cpp
    tst_track_editing.cpp
    tst_parameter_editing.cpp
    tst_audio_export.cpp
    tst_settings_pages.cpp
    tst_speaker_mix.cpp
)
target_link_libraries(${PROJECT_NAME} PRIVATE EditorRuntime)
add_dependencies(${PROJECT_NAME} DsEditorLite)
lite_register_test(${PROJECT_NAME} CATEGORY gui GUI TIMEOUT 90)
