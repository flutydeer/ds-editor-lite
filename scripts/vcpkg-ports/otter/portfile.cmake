# otter provides analyzer interpreters. Model packages are installed separately.

vcpkg_from_git(
    OUT_SOURCE_PATH SOURCE_PATH
    URL https://github.com/diffscope/otter.git
    REF 95e58bfc4dff9152807600368fd35d12e5183a09
    HEAD_REF spec2.4-uptake
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        onnx WITH_ONNX
)

# Disable optional dsinfer discovery when the feature is off, even if another port installed it.
set(_otter_disable_dsinfer ON)
if(WITH_ONNX)
    set(_otter_disable_dsinfer OFF)
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DOTTER_BUILD_TESTS:BOOL=OFF
        -DOTTER_DISABLE_DSINFER:BOOL=${_otter_disable_dsinfer}
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
    PACKAGE_NAME otter
    CONFIG_PATH lib/cmake/otter
)

# Plugins install DLLs under lib/plugins and are loaded dynamically.
set(VCPKG_POLICY_ALLOW_DLLS_IN_LIB enabled)

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
)

vcpkg_copy_pdbs(BUILD_PATHS
    "${CURRENT_PACKAGES_DIR}/bin/*.dll"
    "${CURRENT_PACKAGES_DIR}/debug/bin/*.dll"
    "${CURRENT_PACKAGES_DIR}/lib/plugins/*.dll"
    "${CURRENT_PACKAGES_DIR}/debug/lib/plugins/*.dll"
)
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
