project(TestVoicebankAudit)

# Deliberately not linking the SynthrtEngine library: this recompiles the four files it needs, so
# it builds while the rest of that library is still being moved to the main line.
lite_add_test(${PROJECT_NAME}
        SOURCES
        main.cpp
        ../../libs/SynthrtEngine/VoicebankCatalog.cpp
        ../../libs/SynthrtEngine/LanguageBridge.cpp
        ../../libs/SynthrtEngine/SingerPipeline.cpp
        ../../libs/SynthrtEngine/SynthrtBootstrap.cpp
)

find_package(synthrt CONFIG REQUIRED)
find_package(wolf CONFIG REQUIRED)
find_package(otter CONFIG REQUIRED)
include(${LITE_CMAKE_DIR}/LiteDsinfer.cmake)

target_include_directories(${PROJECT_NAME} PRIVATE ../../libs/SynthrtEngine ../../libs)

target_link_libraries(${PROJECT_NAME} PRIVATE
        synthrt::synthrt
        wolf::wolf
        otter::otter
        lite::dsinfer
)

# A published voicebank is far too large to keep in the tree, so this names one instead. Set
# LITE_AUDIT_VOICEBANK to a directory already converted by scripts/convert-voicebank.py, and
# LITE_AUDIT_LANGUAGE_PACKAGES to the wolf language packages it was converted against. Without
# both the test skips, which is the honest outcome: nothing is broken when there is no voicebank.

target_compile_definitions(${PROJECT_NAME} PRIVATE
        TEST_VOICEBANK_DIR="${LITE_AUDIT_VOICEBANK}"
        TEST_LANGUAGE_PACKAGES="${LITE_AUDIT_LANGUAGE_PACKAGES}"
        TEST_PLUGIN_ROOT="${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib"
        TEST_ONNXRUNTIME_DIR="${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/onnxruntime-builds/runtime/default"
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
