project(TestEditorInteraction)

lite_add_test(${PROJECT_NAME} SOURCES
    test_main.cpp tst_editor_interaction.h
    tst_controllers.cpp tst_viewport.cpp tst_edge_scroll.cpp tst_piano_roll.cpp tst_tracks.cpp tst_scroll_bars.cpp tst_shortcuts.cpp
)
target_link_libraries(${PROJECT_NAME} PRIVATE EditorGuiCore AutomationTestSupport)
lite_register_test(${PROJECT_NAME} CATEGORY gui GUI TIMEOUT 60)
