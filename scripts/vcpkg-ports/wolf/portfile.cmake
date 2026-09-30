# wolf: the language domain that the editor uses to convert lyrics. Its plugins install under
# lib/plugins/wolf/<category>, beside the plugins of dsinfer and otter, in the single tree that the
# editor deploys and passes to SynthUnit::setPluginPaths.
#
# The language resource packages are data and are not provided by this port. They are released
# separately and are installed by wolf's wolf-lang-packages port or located through
# LITE_WOLF_LANG_PACKAGES.

# A fetch by commit requires no archive hash. The pin is a commit so that a rebuild uses the same
# sources. HEAD_REF names the branch that contains the commit.
vcpkg_from_git(
    OUT_SOURCE_PATH SOURCE_PATH
    URL https://github.com/diffscope/wolf.git
    REF 876482ec7033e8984c10b581f8358bcc435da9d7
    HEAD_REF linguistic-level-1-v2
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        onnx WITH_ONNX
)

# wolf finds dsinfer with a QUIET find_package and builds the model-backed variant if dsinfer is
# found. Without the feature, dsinfer may still be present in the tree because another port
# installed it. dsinfer is therefore disabled explicitly, independent of the contents of the tree.
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

# The interpreters and the linguist provider are plugins located by name at run time and are not
# link targets. On Windows they install their DLLs under lib/plugins.
set(VCPKG_POLICY_ALLOW_DLLS_IN_LIB enabled)

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
)

vcpkg_copy_pdbs()
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
