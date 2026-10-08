#ifndef DEPLOYLAYOUT_H
#define DEPLOYLAYOUT_H

#include <string_view>

/// Relative paths, below the plugin root, of the deployed directories that the engine locates by
/// path.
///
/// The deployment in cmake/LiteBuildApi.cmake copies the plugin trees of dsinfer, wolf and otter
/// into place, and the engine locates the directories below at run time. The CMake side, the
/// packaging scripts in packaging/windows and the engine all read the values from this file. The
/// CMake side and the packaging scripts parse the definitions below with a regular expression, so
/// every definition must keep the exact form <tt>inline constexpr char NAME[] = "value";</tt> on
/// one line.
///
/// A plugin category directory is named after the source directory in which the library builds
/// its plugins. None of the three libraries exports that name, so each name is recorded here once.
/// LiteBuildApi.cmake warns at configure time if an installed plugin tree lacks one of these
/// directories.
namespace lite::synthrt::layout {

    /// The three library level directory names under plugins/, space separated. CMake and the two
    /// packaging scripts parse this file and copy and assert the plugin trees from this list, so the
    /// three names are defined only here and one edit changes every consumer. The category directory
    /// constants below are direct children of these library directories, so both have one source.
    inline constexpr char PLUGIN_LIBRARIES[] = "dsinfer wolf otter";

    /// The dsinfer interpreters of the inference category.
    inline constexpr char DSINFER_INFERENCE_DIR[] = "plugins/dsinfer/inferenceinterpreters";

    /// The wolf interpreters of the inference category.
    inline constexpr char WOLF_INFERENCE_DIR[] = "plugins/wolf/inferenceinterpreters";

    /// The otter interpreters of the inference category, which provide the analyzers.
    inline constexpr char OTTER_INFERENCE_DIR[] = "plugins/otter/inferenceinterpreters";

    /// The dsinfer providers of the singer category.
    inline constexpr char DSINFER_SINGER_DIR[] = "plugins/dsinfer/singerproviders";

    /// The wolf providers of the linguist category.
    inline constexpr char WOLF_LINGUIST_DIR[] = "plugins/wolf/linguistproviders";

    /// The directory of the dsinfer inference driver plugins, which the driver factory searches.
    inline constexpr char INFERENCE_DRIVER_DIR[] = "plugins/dsinfer/inferencedrivers";

    /// The directory of the ONNX Runtime payload that is passed to the driver.
    inline constexpr char ONNX_RUNTIME_DIR[] = "plugins/dsinfer/inferencedrivers/onnx/runtime";

    /// The subdirectory of ONNX_RUNTIME_DIR that holds the CUDA flavor of the payload. Its DLLs
    /// have the same names as the DLLs of the default flavor, so the two flavors cannot share a
    /// directory.
    inline constexpr char CUDA_RUNTIME_SUBDIR[] = "cuda";

    /// The directory of the wolf language packages through which voicebank dependencies resolve.
    inline constexpr char LANGUAGE_PACKAGES_DIR[] = "wolf/packages";

    // The runtime payload is staged beside the driver plugin that loads it.
    static_assert(std::string_view(ONNX_RUNTIME_DIR).starts_with(INFERENCE_DRIVER_DIR),
                  "ONNX_RUNTIME_DIR must lie below INFERENCE_DRIVER_DIR");

}

#endif // DEPLOYLAYOUT_H
