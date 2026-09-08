project(TestHeadlessResources)

lite_add_test(${PROJECT_NAME}
    SOURCES main.cpp ../TestSupport/ProcessFixture.cpp
)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network)
find_package(SndFile CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    SndFile::sndfile
)
add_dependencies(${PROJECT_NAME} DsEditorLite)
lite_register_test(${PROJECT_NAME} CATEGORY workflow RESOURCE_TEST TIMEOUT 600
    ARGS --editor "$<TARGET_FILE:DsEditorLite>"
)
set_tests_properties(${PROJECT_NAME} PROPERTIES
    SKIP_REGULAR_EXPRESSION "Set DSEL_TEST_VOICEBANK_ROOT")
