project(TestApplicationGui)

lite_add_test(${PROJECT_NAME} SOURCES
    test_main.cpp
    tst_application_gui.h
    tst_piano_roll.cpp
    tst_track_editing.cpp
    tst_parameter_editing.cpp
    tst_audio_export.cpp
    tst_settings_pages.cpp
    tst_project_import.cpp
    tst_fill_lyric.cpp
    tst_note_dialogs.cpp
    tst_dynamic_mix.cpp
    tst_speaker_mix.cpp
    tst_resources.cpp
    tst_main_window.cpp
    tst_log_window.cpp
    tst_document_workflow.cpp
    tst_tagger_settings.cpp
    tst_piano_keyboard.cpp
    tst_track_clipboard.cpp
    tst_synth_preview.cpp
    tst_audio_midi_settings.cpp
    tst_track_controls.cpp
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
lite_register_test(${PROJECT_NAME} CATEGORY gui GUI TIMEOUT 90)
