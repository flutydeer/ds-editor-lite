project(TestModelResources)

lite_add_test(${PROJECT_NAME}
    SOURCES tst_model_resources.cpp ../TestSupport/ProcessFixture.cpp
        tst_analyzer_packages.cpp tst_otter_extraction.cpp
        tst_voicebank_languages.cpp tst_voicebank_audit.cpp
)
find_package(Qt6 REQUIRED COMPONENTS Core Network)
find_package(SndFile CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core Qt6::Network
    SndFile::sndfile EditorInferenceCore lite::SynthrtEngine
)
add_dependencies(${PROJECT_NAME} DsEditorLite)
lite_register_test(${PROJECT_NAME} CATEGORY workflow RESOURCE_TEST TIMEOUT 600
    ARGS --editor "$<TARGET_FILE:DsEditorLite>"
)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(LITE_OTTER_FIXTURES "" CACHE PATH "Optional otter model fixtures")
set(LITE_AUDIT_VOICEBANK "" CACHE PATH "Optional published spec 2.4 voicebank")
set(LITE_AUDIT_LANGUAGE_PACKAGES "" CACHE PATH "Language packages for the optional audit")
if(APPLE)
    set(_spec24_plugin_root "$<TARGET_BUNDLE_DIR:DsEditorLite>/Contents/PlugIns")
elseif(WIN32)
    set(_spec24_plugin_root "$<TARGET_FILE_DIR:DsEditorLite>")
else()
    set(_spec24_plugin_root "$<TARGET_FILE_DIR:DsEditorLite>/../lib")
endif()
target_include_directories(${PROJECT_NAME} PRIVATE ../../app ../../libs/SynthrtEngine)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    TEST_PYTHON="${Python3_EXECUTABLE}"
    TEST_SCRIPT="${CMAKE_SOURCE_DIR}/scripts/convert-voicebank.py"
    TEST_OTTER_FIXTURES="${LITE_OTTER_FIXTURES}"
    TEST_VOICEBANK_DIR="${LITE_AUDIT_VOICEBANK}"
    TEST_LANGUAGE_PACKAGES="${LITE_AUDIT_LANGUAGE_PACKAGES}"
    TEST_PLUGIN_ROOT="${_spec24_plugin_root}"
    TEST_ONNXRUNTIME_DIR="${_spec24_plugin_root}/plugins/dsinfer/inferencedrivers/onnx/runtime"
)
