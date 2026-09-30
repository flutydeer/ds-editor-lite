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
# The ONNX Runtime payload's directory is declared by the package that ships it.
find_package(onnxruntime-builds CONFIG REQUIRED)
include(${LITE_CMAKE_DIR}/LiteDsinfer.cmake)

target_include_directories(${PROJECT_NAME} PRIVATE ../../libs/SynthrtEngine ../../libs)

target_link_libraries(${PROJECT_NAME} PRIVATE
        synthrt::synthrt
        wolf::wolf
        otter::otter
        dsinfer::dsinfer
)

# A published voicebank is far too large to keep in the tree, so this names one instead. Set
# LITE_AUDIT_VOICEBANK to a directory already converted by scripts/convert-voicebank.py, and
# LITE_AUDIT_LANGUAGE_PACKAGES to the wolf language packages it was converted against. Without
# both the test skips, which is the honest outcome: nothing is broken when there is no voicebank.

# Where the plugins are. Windows stages the ONNX Runtime beside the driver and nowhere else
# (LiteBuildApi.cmake puts it there, and SynthrtEngine::defaultRuntimePath() agrees), so the only
# tree that can create a driver is the deployed tree that contains this test. On other platforms
# the installed tree is sufficient. Its root is the directory that holds plugins/, two levels above
# the plugin directory that the dsinfer package exports. A Debug build of this test must load the
# debug tree if the package installed a debug tree.
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
# Without a voicebank and its language packages the test has checked nothing, and ctest reports
# that as skipped rather than as a pass.
set_tests_properties(${PROJECT_NAME} PROPERTIES SKIP_RETURN_CODE 77)
