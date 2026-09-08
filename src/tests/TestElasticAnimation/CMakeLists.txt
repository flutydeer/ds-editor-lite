project(TestElasticAnimation)

lite_add_test(${PROJECT_NAME} SOURCES main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE lite::GUI)
lite_register_test(${PROJECT_NAME} CATEGORY gui TIMEOUT 20)
