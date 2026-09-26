project(TestDocumentIO)


lite_add_test(${PROJECT_NAME} SOURCES
    tst_document_io.h
    tst_history_and_paths.cpp
    tst_lifecycle.cpp
    tst_midi_import.cpp
    tst_publication.cpp
    test_main.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE ../../app)
target_link_libraries(${PROJECT_NAME} PRIVATE AutomationTestSupport EditorRuntime lite::ProjectConverters)
lite_register_test(${PROJECT_NAME} CATEGORY workflow)
