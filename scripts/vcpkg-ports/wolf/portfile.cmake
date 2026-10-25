# wolf provides language interpreters. Resources are supplied by wolf-lang-packages.

vcpkg_from_git(
    OUT_SOURCE_PATH SOURCE_PATH
    URL https://github.com/diffscope/wolf.git
    REF a6f94af8d7de0b588b8e3bb5d69ed6025e395121
    HEAD_REF spec2.4-uptake
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        onnx WITH_ONNX
)

# Disable optional dsinfer discovery when the feature is off, even if another port installed it.
set(_wolf_disable_dsinfer ON)
if(WITH_ONNX)
    set(_wolf_disable_dsinfer OFF)
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DWOLF_BUILD_TESTS:BOOL=OFF
        -DWOLF_DISABLE_DSINFER:BOOL=${_wolf_disable_dsinfer}
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
    PACKAGE_NAME wolf
    CONFIG_PATH lib/cmake/wolf
)

# Runtime-loaded plugins install DLLs under lib/plugins.
set(VCPKG_POLICY_ALLOW_DLLS_IN_LIB enabled)

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
)

# Plugins install into lib/plugins/<library>/<category>/, so a single-level glob matches nothing
# and silently leaves the plugin DLLs without debug symbols. Collect them recursively instead.
file(GLOB_RECURSE _plugin_dlls
    "${CURRENT_PACKAGES_DIR}/lib/plugins/*.dll"
    "${CURRENT_PACKAGES_DIR}/debug/lib/plugins/*.dll"
)
vcpkg_copy_pdbs(BUILD_PATHS
    "${CURRENT_PACKAGES_DIR}/bin/*.dll"
    "${CURRENT_PACKAGES_DIR}/debug/bin/*.dll"
    ${_plugin_dlls}
)
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
