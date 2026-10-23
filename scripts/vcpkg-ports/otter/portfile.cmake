# otter provides analyzer interpreters. Model packages are installed separately.

set(_otter_platform_patches)
if(VCPKG_TARGET_IS_OSX)
    list(APPEND _otter_platform_patches macos-pipe-nosigpipe.patch)
endif()

vcpkg_from_git(
    OUT_SOURCE_PATH SOURCE_PATH
    URL https://github.com/diffscope/otter.git
    REF e770ab94ac0a27a31c6cec502aeac765c9db70ef
    HEAD_REF spec2.4-uptake
    PATCHES ${_otter_platform_patches}
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

vcpkg_copy_pdbs()
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
