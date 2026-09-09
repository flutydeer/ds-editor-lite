project(TestApplicationWorkflows)

lite_add_test(${PROJECT_NAME} SOURCES main.cpp ../TestSupport/ProcessFixture.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE EditorRuntime)
add_dependencies(${PROJECT_NAME} DsEditorLite)
lite_register_test(${PROJECT_NAME} CATEGORY workflow TIMEOUT 90)
