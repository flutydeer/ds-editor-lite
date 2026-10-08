# synthrt, main line: the engine on which the editor is built and, with the onnx feature, dsinfer
# and its ONNX Runtime driver. wolf and otter are layered on this package, so the synthrt, wolf and
# otter ports in this directory use synthrt from this port instead of each carrying a separate pin.
#
# Rationale: the shared overlay submodule carries a synthrt port that pins the refactor line, and
# both lines install lib/libsynthrt.so. This overlay is listed first in the manifest, so this port
# shadows the shared port and the editor's tree contains the main line only.

# A fetch by commit requires no archive hash. The pin is a commit rather than the branch tip so that
# a rebuild uses the same sources. HEAD_REF names the branch that contains the commit, along which a
# maintainer advances the pin.
vcpkg_from_git(
    OUT_SOURCE_PATH SOURCE_PATH
    URL https://github.com/diffscope/synthrt.git
    REF 7ea424a056aa776b6cfb55890096629a04e62e22
    HEAD_REF spec2.4-uptake
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        onnx    WITH_ONNX
        cuda12  WITH_CUDA
)

# dsinfer runs the models, so it follows the onnx feature. The editor always requests the feature.
# The feature exists so that a tree that requires only the framework does not build a driver that it
# never loads.
set(_synthrt_dsinfer OFF)
if(WITH_ONNX OR WITH_CUDA)
    set(_synthrt_dsinfer ON)
endif()

set(_synthrt_cuda OFF)
if(WITH_CUDA)
    set(_synthrt_cuda ON)
endif()

# DirectML is a Windows API. On other platforms dsinfer runs the CPU provider, so the option is
# enabled only on Windows instead of relying on dsinfer to ignore it elsewhere.
set(_synthrt_directml OFF)

if(VCPKG_TARGET_IS_WINDOWS)
    set(_synthrt_directml ON)
endif()

# Nothing is staged for ONNX Runtime. synthrt finds the onnxruntime-builds package itself, and the
# feature's dependency has already installed it into this same tree, so the headers and the payload
# are in the location that find_package searches.
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DSYNTHRT_BUILD_TESTS:BOOL=OFF
        -DSYNTHRT_BUILD_DSINFER:BOOL=${_synthrt_dsinfer}
        -DDSINFER_ENABLE_CUDA:BOOL=${_synthrt_cuda}
        # The editor ships the DirectML provider on Windows.
        -DDSINFER_ENABLE_DIRECTML:BOOL=${_synthrt_directml}
)

vcpkg_cmake_install()

# synthrt installs two CMake packages side by side under lib/cmake: synthrt itself and, if dsinfer
# is built, dsinfer. The fixup of either package must not delete the parent directory. Otherwise the
# other package is silently lost, and consumers must locate the dsinfer library manually.
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

# The ONNX driver is not a link target. Like every driver, it is loaded at run time as a plugin
# found on a search path, under lib/plugins/dsinfer/inferencedrivers. On Windows the plugins install
# their DLLs there, and this policy prevents vcpkg's layout check from rejecting them.
set(VCPKG_POLICY_ALLOW_DLLS_IN_LIB enabled)

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
)

vcpkg_copy_pdbs()
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
