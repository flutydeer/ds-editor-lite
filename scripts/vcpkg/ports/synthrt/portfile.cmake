set(VCPKG_POLICY_ALLOW_DLLS_IN_LIB enabled)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        cuda11 WITH_CUDA11
        cuda12 WITH_CUDA12
)

if(${WITH_CUDA11} AND ${WITH_CUDA12})
    message(FATAL_ERROR "Cannot enable both cuda11 and cuda12 at the same time")
endif()

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO diffscope/synthrt
    REF 020851a3c34a3fb51e63415ef899aa53aadd3c75
    SHA512 0eb3fd296a01b0dc683c6ea90f7f6b71278f5bfbc75bd4260d5c4a9d6383a50d2aeb98527d9d8abc1f8bbb655d5b115230a595a6ca8519bb936d5c8561a2e046
    HEAD_REF main
)

# Download ONNX Runtime
include("${CMAKE_CURRENT_LIST_DIR}/setup-onnxruntime/setup.cmake")
set(_ort_deploy_prefix "${SOURCE_PATH}/third-party/onnxruntime")
message(STATUS "OS: ${_os_display_name}")
if(VCPKG_TARGET_IS_WINDOWS)
    message(STATUS "Downloading DirectML version of ONNX Runtime...")
    set(_ort_deploy_suffix "default")
    set(_deploy_dst_dir "${_ort_deploy_prefix}/${_ort_deploy_suffix}")
    download_onnxruntime_from_nuget("${_deploy_dst_dir}")
    download_dml_from_nuget("${_deploy_dst_dir}")
else()
    message(STATUS "Downloading CPU version of ONNX Runtime...")
    set(_ort_deploy_suffix "default")
    set(_deploy_dst_dir "${_ort_deploy_prefix}/${_ort_deploy_suffix}")
    download_onnxruntime_from_github("cpu" "${_deploy_dst_dir}")
endif()

# Optional GPU (CUDA) version download
if(${WITH_CUDA12})
    message(STATUS "Downloading GPU (CUDA 12.x) version of ONNX Runtime...")
    set(_ort_deploy_suffix "cuda")
    set(_deploy_dst_dir "${_ort_deploy_prefix}/${_ort_deploy_suffix}")
    download_onnxruntime_from_github("cuda12" "${_deploy_dst_dir}")
elseif(${WITH_CUDA11})
    message(STATUS "Downloading GPU (CUDA 11.x) version of ONNX Runtime...")
    set(_ort_deploy_suffix "cuda")
    set(_deploy_dst_dir "${_ort_deploy_prefix}/${_ort_deploy_suffix}")
    download_onnxruntime_from_github("cuda11" "${_deploy_dst_dir}")
endif()

# Configure synthrt
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DSYNTHRT_BUILD_TESTS:BOOL=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(
    PACKAGE_NAME
        synthrt
    CONFIG_PATH
        lib/cmake/synthrt
    # DO NOT omit `DO_NOT_DELETE_PARENT_CONFIG_PATH` option!
    # Otherwise dsinfer config will be also deleted,
    # failing the next command (fix up dsinfer config).
    DO_NOT_DELETE_PARENT_CONFIG_PATH
)
vcpkg_cmake_config_fixup(
    PACKAGE_NAME
        dsinfer
    CONFIG_PATH
        lib/cmake/dsinfer
)
vcpkg_copy_pdbs()

vcpkg_copy_tools(
    TOOL_NAMES dsinfer-cli
    SEARCH_DIR ${CURRENT_PACKAGES_DIR}/bin
    AUTO_CLEAN
)

vcpkg_copy_tool_dependencies("${CURRENT_PACKAGES_DIR}/tools/${PORT}")

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
