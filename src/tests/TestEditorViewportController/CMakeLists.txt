project(TestEditorViewportController)

lite_add_test(${PROJECT_NAME}
    SOURCES
        main.cpp
        ../../app/UI/Views/Common/EditorViewportAnimation.cpp
        ../../app/UI/Views/Common/EditorViewportController.cpp
        ../../app/UI/Views/ClipEditor/PianoRoll/PianoRollCoord.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE ../../app)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core lite::GUI)
lite_register_test(${PROJECT_NAME} CATEGORY gui)
