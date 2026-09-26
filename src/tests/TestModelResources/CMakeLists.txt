project(TestModelResources)

lite_add_test(${PROJECT_NAME}
    SOURCES tst_model_resources.cpp ../TestSupport/ProcessFixture.cpp
)
find_package(Qt6 REQUIRED COMPONENTS Core Network)
find_package(SndFile CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core Qt6::Network
    SndFile::sndfile
)
add_dependencies(${PROJECT_NAME} DsEditorLite)
lite_register_test(${PROJECT_NAME} CATEGORY workflow RESOURCE_TEST TIMEOUT 600
    ARGS --editor "$<TARGET_FILE:DsEditorLite>"
)
