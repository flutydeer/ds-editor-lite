# synthrt, main line, overriding the shared overlay's synthrt port, which pins the refactor line.
# ds-editor-lite builds against the main line together with wolf and otter, which are layered on it.

# The onnxruntime-builds-uptake branch carries the ONNX Runtime package uptake and the singer
# category fields the editor and wolf read (languages, defaultLanguage, reservedPhonemes). A fetch
# by commit needs no archive hash.
vcpkg_from_git(
    OUT_SOURCE_PATH SOURCE_PATH
    URL https://github.com/diffscope/synthrt.git
    REF ba5f1779ad05a4c63e3ade36aeb8c57636a67ac6
    HEAD_REF onnxruntime-builds-uptake
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        onnx WITH_ONNX
        cuda12 WITH_CUDA
)

# dsinfer is off unless asked for. The inference and singer categories live in synthrt itself, and
# the onnx feature turns dsinfer and its ONNX driver on for synthesis and analysis.
#
# Nothing is staged for ONNX Runtime. synthrt finds the onnxruntime-builds package itself, and the
# feature's dependency has already installed it into this same tree, so the headers are where
# find_package looks.
set(_synthrt_dsinfer OFF)

if(WITH_ONNX)
    set(_synthrt_dsinfer ON)
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DSYNTHRT_BUILD_TESTS:BOOL=OFF
        -DSYNTHRT_BUILD_DSINFER:BOOL=${_synthrt_dsinfer}
        # The CUDA execution provider is compiled in only with the cuda12 feature, which also brings
        # the CUDA flavour of the ONNX Runtime payload. Without it Windows runs the DirectML
        # provider and every other platform the CPU one.
        -DDSINFER_ENABLE_CUDA:BOOL=${WITH_CUDA}
        -DDSINFER_ENABLE_DIRECTML:BOOL=ON
)

vcpkg_cmake_install()

# synthrt installs two CMake packages side by side under lib/cmake: synthrt itself and, with the
# onnx feature, dsinfer. The fixup for one must not delete the parent directory, or the other
# package is silently lost and consumers are left locating the dsinfer library by hand.
vcpkg_cmake_config_fixup(
    PACKAGE_NAME synthrt
    CONFIG_PATH lib/cmake/synthrt
    DO_NOT_DELETE_PARENT_CONFIG_PATH
)
if(WITH_ONNX)
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

# The ONNX driver is not a link target. It is reached the way every driver is: as a plugin found
# on a search path at run time, under lib/plugins/dsinfer/inferencedrivers. The plugins install
# their DLLs there on Windows, which vcpkg's layout check would otherwise refuse.
set(VCPKG_POLICY_ALLOW_DLLS_IN_LIB enabled)

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
)

vcpkg_copy_pdbs()
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
