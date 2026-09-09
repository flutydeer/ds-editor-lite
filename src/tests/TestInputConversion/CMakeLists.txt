project(TestInputConversion)

lite_add_test(${PROJECT_NAME}
    SOURCES main.cpp InputConversionTests.h SpeakerMixValidation.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE EditorInferenceCore)
lite_register_test(${PROJECT_NAME} CATEGORY unit)
