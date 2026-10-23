# This overlay selects the synthrt version used by the editor, wolf, and otter.

vcpkg_from_git(
    OUT_SOURCE_PATH SOURCE_PATH
    URL https://github.com/diffscope/synthrt.git
    REF ec52820c60829cd3735e2510a6948b0ee22c1c87
    HEAD_REF spec2.4-uptake
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        onnx    WITH_ONNX
        cuda12  WITH_CUDA
)

set(_synthrt_dsinfer OFF)
if(WITH_ONNX OR WITH_CUDA)
    set(_synthrt_dsinfer ON)
endif()

set(_synthrt_cuda OFF)
if(WITH_CUDA)
    set(_synthrt_cuda ON)
endif()

set(_synthrt_directml OFF)

if(VCPKG_TARGET_IS_WINDOWS)
    set(_synthrt_directml ON)
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DSYNTHRT_BUILD_TESTS:BOOL=OFF
        -DSYNTHRT_BUILD_DSINFER:BOOL=${_synthrt_dsinfer}
        -DDSINFER_ENABLE_CUDA:BOOL=${_synthrt_cuda}
        -DDSINFER_ENABLE_DIRECTML:BOOL=${_synthrt_directml}
)

vcpkg_cmake_install()

# Fix up synthrt and dsinfer without removing their shared lib/cmake parent.
vcpkg_cmake_config_fixup(
    PACKAGE_NAME synthrt
    CONFIG_PATH lib/cmake/synthrt
    DO_NOT_DELETE_PARENT_CONFIG_PATH
)
if(_synthrt_dsinfer)
    vcpkg_cmake_config_fixup(
        PACKAGE_NAME dsinfer
        CONFIG_PATH lib/cmake/dsinfer
        DO_NOT_DELETE_PARENT_CONFIG_PATH
    )
endif()
file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/lib/cmake"
    "${CURRENT_PACKAGES_DIR}/debug/lib/cmake"
)

# Runtime-loaded driver DLLs install under lib/plugins.
set(VCPKG_POLICY_ALLOW_DLLS_IN_LIB enabled)

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
)

vcpkg_copy_pdbs()
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
