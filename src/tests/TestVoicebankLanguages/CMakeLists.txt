project(TestVoicebankLanguages)

lite_add_test(${PROJECT_NAME} SOURCES main.cpp)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)
# The converter runs on the interpreter found by CMake, because Windows installations do not
# provide the POSIX executable name.
find_package(Python3 COMPONENTS Interpreter REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::Core)

target_compile_definitions(${PROJECT_NAME} PRIVATE
        TEST_PYTHON="${Python3_EXECUTABLE}"
        TEST_SCRIPT="${CMAKE_SOURCE_DIR}/scripts/convert-voicebank.py"
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
