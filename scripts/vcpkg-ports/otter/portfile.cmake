# otter: the analysis domain that the editor uses to extract pitch and notes. Its analysers are
# inference modules, and their interpreters install under lib/plugins/otter/inferenceinterpreters,
# beside the interpreters of dsinfer and wolf.
#
# The analyzer packages are data and are not provided by this port. The model weights are too large
# for a port to carry, and the user selects the analyzer among the installed packages.

# A fetch by commit requires no archive hash. The pin is a commit so that a rebuild uses the same
# sources. HEAD_REF names the branch that contains the commit.
vcpkg_from_git(
    OUT_SOURCE_PATH SOURCE_PATH
    URL https://github.com/diffscope/otter.git
    REF f4820d99cd72ee4a21baceec5be658a80d231d50
    HEAD_REF analysis-level-1
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        onnx WITH_ONNX
)

# The contract library requires neither dsinfer nor a driver; only the interpreters require them.
# otter finds dsinfer with a QUIET find_package. Without the feature, dsinfer is therefore disabled
# explicitly, independent of any other port that installed dsinfer into the tree.
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

# The interpreters are plugins located by name at run time and are not link targets. On Windows
# they install their DLLs under lib/plugins.
set(VCPKG_POLICY_ALLOW_DLLS_IN_LIB enabled)

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
)

vcpkg_copy_pdbs()
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
