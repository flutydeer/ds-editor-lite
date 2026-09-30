project(TestAnalyzerPackages)

# Hosts query analyzers through the engine facade; therefore the test links the engine library that
# the editor links instead of re-implementing the lookup.
lite_add_test(${PROJECT_NAME}
        SOURCES
        main.cpp
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

target_include_directories(${PROJECT_NAME} PRIVATE ../../libs/SynthrtEngine ../../libs)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        lite::SynthrtEngine
)

# The test writes the packages under test into a temporary directory that it owns. The test checks
# which directory the engine scans and how the engine handles the packages found there. A fixture
# produced by another build is unsuitable, because the test would be skipped on the machines that
# require it.
#
# The plugin tree and ONNX Runtime are loaded from the deployment location next to the executable,
# which is also the location used by the editor. The post-build steps of the DsEditorLite target
# deploy them. In a tree in which only the tests were built (for example, by a CI job that builds
# only the test targets), the test is skipped on every run instead of reporting a failure that is
# indistinguishable from a real failure. Build DsEditorLite before running the tests to enable
# this test.
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
set_tests_properties(${PROJECT_NAME} PROPERTIES SKIP_RETURN_CODE 77)
