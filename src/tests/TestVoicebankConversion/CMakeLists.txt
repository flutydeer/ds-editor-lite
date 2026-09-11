project(TestVoicebankConversion)

lite_add_test(${PROJECT_NAME} SOURCES main.cpp)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::Core)

target_compile_definitions(${PROJECT_NAME} PRIVATE
        TEST_FIXTURE_DIR="${CMAKE_CURRENT_BINARY_DIR}/../../../src/tests/resources/ci-fixture@1.0.0"
        TEST_SCRIPT="${CMAKE_SOURCE_DIR}/scripts/convert-voicebank.py"
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
