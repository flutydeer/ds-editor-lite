set(VCPKG_POLICY_ALLOW_DLLS_IN_LIB enabled)
set(VCPKG_POLICY_SKIP_LIB_CMAKE_MERGE_CHECK enabled)
set(VCPKG_POLICY_SKIP_MISPLACED_CMAKE_FILES_CHECK enabled)
set(VCPKG_POLICY_ALLOW_EMPTY_FOLDERS enabled)

# The cuda12 feature only fans out to the onnxruntime-builds[cuda12]
# dependency (see vcpkg.json); the ONNX Runtime package itself is located via
# find_package(onnxruntime-builds) in the synthrt sources, so nothing is
# forwarded to vcpkg_cmake_configure here.

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO diffscope/synthrt
    REF 814bf81e6cd86b6670b2635032d842a997001a55
    SHA512 7c33737b6913c491fa7dc538ac8639b52efaaf648418b1b16f1745e0b5e518d6484e9eaa9a45040efb753ef3e16f881b09954c29b6f73475e092818dcfa78c33
    HEAD_REF localization/passthrough-keys
    PATCHES include-mutex.patch
)

# ONNX Runtime comes from the onnxruntime-builds port (dependency); synthrt's
# own CMake does find_package(onnxruntime-builds) and reads
# ONNXRUNTIME_BUILDS_INCLUDE_DIR / RUNTIME_DIR directly, so no path is passed
# here. (split: ort download/deploy logic moved to ports/onnxruntime-builds)

# Configure synthrt
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DSYNTHRT_BUILD_TESTS:BOOL=OFF
        -DSYNTHRT_BUILD_TOOLS:BOOL=OFF
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

# Remove stale INTERFACE_INCLUDE_DIRECTORIES entries pointing to
# ${_IMPORT_PREFIX}/../../include (not valid for vcpkg layout).
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
# Copy PDBs next to every deployed DLL. The default BUILD_PATHS only covers
# bin/ (and debug/bin), which misses the plugin tree under lib/plugins/ —
# lite deploys those plugin DLLs verbatim, so their PDBs must sit beside them
# for Debug/RelWithDebInfo installs (Release-family installs sweep them again;
# see lite's cmake/LiteBuildApi.cmake).
vcpkg_copy_pdbs(
    BUILD_PATHS
        "${CURRENT_PACKAGES_DIR}/bin/*.dll"
        "${CURRENT_PACKAGES_DIR}/debug/bin/*.dll"
        "${CURRENT_PACKAGES_DIR}/lib/plugins/*.dll"
        "${CURRENT_PACKAGES_DIR}/debug/lib/plugins/*.dll"
)

vcpkg_copy_tool_dependencies("${CURRENT_PACKAGES_DIR}/tools/${PORT}")

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
