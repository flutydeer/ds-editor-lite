# wolf: the language domain that the editor uses to convert lyrics. Its plugins install under
# lib/plugins/wolf/<category>, beside the plugins of dsinfer and otter, in the single tree that the
# editor deploys and passes to SynthUnit::setPluginPaths.
#
# The language resource packages are data and are not provided by this port. They are released
# separately and are installed by the wolf-lang-packages overlay port in this directory or located
# through LITE_WOLF_LANG_PACKAGES.

# A commit pin; the HEAD_REF rationale is stated in the synthrt port beside this one.
vcpkg_from_git(
    OUT_SOURCE_PATH SOURCE_PATH
    URL https://github.com/diffscope/wolf.git
    REF eda2b4880da39dee9d9e9cfd7e04caaf951993e7
    HEAD_REF spec2.4-uptake
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
