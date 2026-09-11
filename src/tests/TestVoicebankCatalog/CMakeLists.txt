project(TestVoicebankCatalog)

# Deliberately not linking the SynthrtEngine library: this recompiles the one file it needs, so it
# builds while the rest of that library is still being moved to the main line.
lite_add_test(${PROJECT_NAME}
        SOURCES
        main.cpp
        ../../libs/SynthrtEngine/VoicebankCatalog.cpp
        ../../libs/SynthrtEngine/SingerPipeline.cpp
        ../../libs/SynthrtEngine/SynthrtBootstrap.cpp
)

find_package(synthrt CONFIG REQUIRED)
find_package(wolf CONFIG REQUIRED)
find_package(otter CONFIG REQUIRED)
include(${LITE_CMAKE_DIR}/LiteDsinfer.cmake)

target_include_directories(${PROJECT_NAME} PRIVATE ../../libs/SynthrtEngine)

target_link_libraries(${PROJECT_NAME} PRIVATE
        synthrt::synthrt
        wolf::wolf
        otter::otter
        lite::dsinfer
)

# The package under test is produced by the converter from the editor's own 2.3 fixture, so the
# two are checked against each other rather than against a hand written 2.4 file that could drift.
#
# The 2.3 fixture is produced during a CI build and is not in the tree, so the conversion runs
# only when it is there; without it the test finds no packages and skips. Guarded here rather
# than left to fail, because a build that breaks on a missing test fixture stops being run.
set(_converted ${CMAKE_CURRENT_BINARY_DIR}/packages)
set(_fixture ${CMAKE_BINARY_DIR}/src/tests/resources/ci-fixture@1.0.0)
if(EXISTS ${_fixture}/desc.json)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -rf ${_converted}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${_converted}
            COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/convert-voicebank.py
                    ${_fixture}
                    --output ${_converted}/ci-fixture
            COMMENT "Converting the 2.3 fixture for the voicebank catalog test"
    )
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
        TEST_PACKAGE_DIR="${_converted}"
        TEST_PLUGIN_ROOT="${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib"
        TEST_ONNXRUNTIME_DIR="${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/onnxruntime-builds/runtime/default"
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
