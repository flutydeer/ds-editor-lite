project(TestVoicebankAudit)

# Links the engine library that the editor links, so that the audited catalog, language bridge and
# bootstrap are the implementations that the editor runs.
lite_add_test(${PROJECT_NAME}
        SOURCES
        main.cpp
)

# The package that ships the ONNX Runtime payload declares the payload directory.
find_package(onnxruntime-builds CONFIG REQUIRED)
include(${LITE_CMAKE_DIR}/LiteDsinfer.cmake)

target_include_directories(${PROJECT_NAME} PRIVATE ../../libs/SynthrtEngine ../../libs)

target_link_libraries(${PROJECT_NAME} PRIVATE
        lite::SynthrtEngine
)

# A published voicebank is too large to store in the tree; therefore the voicebank path is a
# configuration variable. Set LITE_AUDIT_VOICEBANK to a directory converted by
# scripts/convert-voicebank.py, and LITE_AUDIT_LANGUAGE_PACKAGES to the wolf language packages used
# for the conversion. If either variable is unset, the test is skipped, because no defect can be
# detected without a voicebank.

# Plugin root. On Windows the ONNX Runtime is staged only next to the driver (LiteBuildApi.cmake
# places it there, consistent with SynthrtEngine::defaultRuntimePath()), so only the deployed tree
# that contains this test can create a driver. On other platforms the installed tree is
# sufficient. Its root is the directory that contains plugins/, two levels above the plugin
# directory exported by the dsinfer package. A Debug build of this test must load the debug tree
# if the package installed a debug tree.
if(WIN32)
    set(lite_audit_plugin_root "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
else()
    set(lite_audit_plugin_root "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib")
    if(DEFINED DSINFER_PLUGINS_DIR)
        set(_lite_audit_plugins "${DSINFER_PLUGINS_DIR}")
        if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND DEFINED DSINFER_PLUGINS_DIR_DEBUG)
            set(_lite_audit_plugins "${DSINFER_PLUGINS_DIR_DEBUG}")
        endif()
        get_filename_component(_lite_audit_plugins "${_lite_audit_plugins}" DIRECTORY)
        get_filename_component(lite_audit_plugin_root "${_lite_audit_plugins}" DIRECTORY)
    endif()
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
        TEST_VOICEBANK_DIR="${LITE_AUDIT_VOICEBANK}"
        TEST_LANGUAGE_PACKAGES="${LITE_AUDIT_LANGUAGE_PACKAGES}"
        TEST_PLUGIN_ROOT="${lite_audit_plugin_root}"
        TEST_ONNXRUNTIME_DIR="${ONNXRUNTIME_BUILDS_RUNTIME_DIR}"
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
# Without a voicebank and its language packages the test checks nothing, and ctest reports the
# test as skipped instead of passed.
set_tests_properties(${PROJECT_NAME} PROPERTIES SKIP_RETURN_CODE 77)
