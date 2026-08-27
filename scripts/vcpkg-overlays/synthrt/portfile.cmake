set(VCPKG_POLICY_ALLOW_DLLS_IN_LIB enabled)
set(VCPKG_POLICY_SKIP_LIB_CMAKE_MERGE_CHECK enabled)
set(VCPKG_POLICY_SKIP_MISPLACED_CMAKE_FILES_CHECK enabled)
set(VCPKG_POLICY_ALLOW_EMPTY_FOLDERS enabled)

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
    REF 399468589386b993213879527ad6453b581e8e1a
    SHA512 cea54a49bf222843d04f5b7e55aae21c91196d1ddf216be1ca02c9184f5896a7b6521eee669a301f076fbd5deb94a3a0a6f1ad851d148102c36b461034533227
    HEAD_REF refactor
    PATCHES
        bound-rmvpe-inference-chunks.patch
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

# Fix up all cmake config packages, preserving parent dir
# so later fixups for sibling packages are not deleted.
vcpkg_cmake_config_fixup(
    PACKAGE_NAME synthrt
    CONFIG_PATH lib/cmake/synthrt
    DO_NOT_DELETE_PARENT_CONFIG_PATH
)
vcpkg_cmake_config_fixup(
    PACKAGE_NAME dsinfer
    CONFIG_PATH lib/cmake/dsinfer
    DO_NOT_DELETE_PARENT_CONFIG_PATH
)

# Individual srt-* sub-packages referenced by dsinfer/synthrt configs
foreach(_pkg IN ITEMS srt-core srt-driver srt-s2p srt-svs srt-g2p srt-c srt-audio srt-extract)
    vcpkg_cmake_config_fixup(
        PACKAGE_NAME ${_pkg}
        CONFIG_PATH lib/cmake/${_pkg}
        DO_NOT_DELETE_PARENT_CONFIG_PATH
    )
endforeach()

# dsbank config is installed at lib/cmake/srt-ds-bank/ (DSBANK_INSTALL_NAME = srt-ds-bank)
vcpkg_cmake_config_fixup(
    PACKAGE_NAME srt-ds-bank
    CONFIG_PATH lib/cmake/srt-ds-bank
    DO_NOT_DELETE_PARENT_CONFIG_PATH
)

# F7 (find_dependency injection) REMOVED:
# All source Config.cmake.in files now declare find_dependency() correctly,
# making F7's post-install injection fully redundant (would create duplicates).
# Verified: srt-core/srt-driver/srt-s2p/srt-svs/srt-g2p/srt-ds-bank/srt-c/
# dsinfer/srt-audio/srt-extract all have correct find_dependency in source.
# P1 added missing srt-s2p to srt-g2p and srt-s2p/srt-svs to srt-c.

# F8: Remove stale INTERFACE_INCLUDE_DIRECTORIES entries pointing to
# ${_IMPORT_PREFIX}/../../include (not valid for vcpkg layout).
# After P0.5 (BuildAPI.cmake fix), this is a no-op but kept as safety net.
foreach(_pkg IN ITEMS srt-core srt-driver srt-s2p srt-svs srt-g2p srt-ds-bank srt-c dsinfer srt-audio srt-extract)
    set(_tf "${CURRENT_PACKAGES_DIR}/share/${_pkg}/${_pkg}Targets.cmake")
    if(EXISTS "${_tf}")
        file(READ "${_tf}" _tc)
        string(REPLACE ";\${_IMPORT_PREFIX}/../../include" "" _tc "${_tc}")
        file(WRITE "${_tf}" "${_tc}")
    endif()
endforeach()

# Fix dsinfer target include directories:
# The dsinfer headers are installed under include/diffsinger/Infer/dsinfer/
# but CommonApiL1.h and other headers use #include <dsinfer/...> (angle-bracket),
# which requires include/diffsinger/Infer/ to be on the include path.
set(_dsinfer_tf "${CURRENT_PACKAGES_DIR}/share/dsinfer/dsinferTargets.cmake")
if(EXISTS "${_dsinfer_tf}")
    file(READ "${_dsinfer_tf}" _tc)
    string(REPLACE
        "INTERFACE_INCLUDE_DIRECTORIES \"\${_IMPORT_PREFIX}/include\""
        "INTERFACE_INCLUDE_DIRECTORIES \"\${_IMPORT_PREFIX}/include;\${_IMPORT_PREFIX}/include/diffsinger/Infer\""
        _tc "${_tc}")
    file(WRITE "${_dsinfer_tf}" "${_tc}")
endif()

# Create srt-ds-infer package: the target is defined inside dsinfer,
# so this package just depends on dsinfer.
file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/share/srt-ds-infer")
file(WRITE "${CURRENT_PACKAGES_DIR}/share/srt-ds-infer/srt-ds-inferConfig.cmake" [[
include(CMakeFindDependencyMacro)
find_dependency(dsinfer)
]])
file(WRITE "${CURRENT_PACKAGES_DIR}/share/srt-ds-infer/srt-ds-inferConfigVersion.cmake" [[
set(PACKAGE_VERSION 1.0.0)
if(PACKAGE_FIND_VERSION VERSION_GREATER PACKAGE_VERSION)
  set(PACKAGE_VERSION_COMPATIBLE FALSE)
else()
  set(PACKAGE_VERSION_COMPATIBLE TRUE)
  if(PACKAGE_FIND_VERSION VERSION_EQUAL PACKAGE_VERSION)
    set(PACKAGE_VERSION_EXACT TRUE)
  endif()
endif()
]])

# Remove lib/cmake since all packages have been migrated to share/
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/lib/cmake")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/lib/cmake")
vcpkg_copy_pdbs()

vcpkg_copy_tool_dependencies("${CURRENT_PACKAGES_DIR}/tools/${PORT}")

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
