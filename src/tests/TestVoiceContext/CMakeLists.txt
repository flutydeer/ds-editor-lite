project(TestVoiceContext)

lite_add_test(${PROJECT_NAME} SOURCES main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE lite::ProjectModel)
lite_register_test(${PROJECT_NAME} CATEGORY unit)
