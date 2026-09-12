project(TestEditorInteraction)

lite_add_test(${PROJECT_NAME} SOURCES
    test_main.cpp tst_editor_interaction.h
    tst_controllers.cpp tst_viewport.cpp tst_edge_scroll.cpp tst_piano_roll.cpp tst_tracks.cpp tst_scroll_bars.cpp tst_shortcuts.cpp
    tst_list_reordering.cpp ../TestSupport/GuiAppFixture.h
)
target_link_libraries(${PROJECT_NAME} PRIVATE EditorRuntime AutomationTestSupport)
add_dependencies(${PROJECT_NAME} DsEditorLite)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    LITE_TEST_APPLICATION_CONFIG_ROOT="$<TARGET_FILE_DIR:DsEditorLite>/configs")
if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_RPATH "$<TARGET_BUNDLE_DIR:DsEditorLite>/Contents/Frameworks")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        LITE_TEST_APPLICATION_PLUGIN_ROOT="$<TARGET_BUNDLE_DIR:DsEditorLite>/Contents/PlugIns")
endif()
lite_register_test(${PROJECT_NAME} CATEGORY gui GUI TIMEOUT 60)
# Offscreen ignores QDrag; minimal supports Qt's in-process drag loop.
set_property(TEST ${PROJECT_NAME} APPEND PROPERTY ENVIRONMENT_MODIFICATION
    "QT_QPA_PLATFORM=set:minimal:enable_fonts" "QT_STYLE_OVERRIDE=set:Fusion")
