project(TestLanguageBridge)

# Deliberately not linking the SynthrtEngine library: this recompiles the one file it needs, so it
# builds while the rest of that library is still being moved to the main line.
lite_add_test(${PROJECT_NAME}
        SOURCES
        main.cpp
        ../../libs/SynthrtEngine/VoicebankCatalog.cpp
        ../../libs/SynthrtEngine/LanguageBridge.cpp
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
# A singer's language has to lead to a linguist in another package, so wolf's own Mandarin fixture
# and the pinyin engine it depends on are staged beside the converted voicebank. WOLF_FIXTURE_DIR
# points at a wolf checkout; without it the test finds no packages and skips.
set(_staged ${CMAKE_CURRENT_BINARY_DIR}/packages)
if(WOLF_FIXTURE_DIR)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -rf ${_staged}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${_staged}
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${WOLF_FIXTURE_DIR}/lang-cmn ${_staged}/lang-cmn
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${WOLF_FIXTURE_DIR}/pinyin-engine ${_staged}/pinyin-engine
            COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/convert-voicebank.py
                    ${CMAKE_BINARY_DIR}/src/tests/resources/ci-fixture@1.0.0
                    --output ${_staged}/ci-fixture
                    --language cmn=wolf/test-lang-cmn:linguist/cmn@1.0.0.0
            COMMENT "Staging a voicebank bound to a language for the bridge test"
    )
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
        TEST_PACKAGE_DIR="${_staged}"
        TEST_PLUGIN_ROOT="${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib"
        TEST_ONNXRUNTIME_DIR="${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/onnxruntime-builds/runtime/default"
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
