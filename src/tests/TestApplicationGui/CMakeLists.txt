project(TestApplicationGui)

lite_add_test(${PROJECT_NAME} SOURCES
    main.cpp
    ApplicationGuiTests.h
    PianoRoll.cpp
    TrackEditing.cpp
    ParameterEditing.cpp
    AudioExport.cpp
)
target_link_libraries(${PROJECT_NAME} PRIVATE EditorRuntime)
add_dependencies(${PROJECT_NAME} DsEditorLite)
lite_register_test(${PROJECT_NAME} CATEGORY gui GUI TIMEOUT 90)
