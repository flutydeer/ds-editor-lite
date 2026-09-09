project(TestEditorRendering)

lite_add_test(${PROJECT_NAME} SOURCES
    test_main.cpp tst_editor_rendering.h tst_glyph_atlas.cpp tst_geometry.cpp tst_waveforms.cpp
)
target_link_libraries(${PROJECT_NAME} PRIVATE EditorGuiCore)
lite_register_test(${PROJECT_NAME} CATEGORY gui GUI)
