#ifndef DEPLOYLAYOUT_H
#define DEPLOYLAYOUT_H

/// Relative paths, below the plugin root, of the deployed directories that the engine locates by
/// path.
///
/// The deployment in cmake/LiteBuildApi.cmake copies these directories into place, and the engine
/// locates them at run time. Both read the values from this file. The CMake side parses the three
/// definitions below, so the definitions must keep this exact form.
namespace lite::synthrt::layout {

    /// The directory of the ONNX Runtime payload that is passed to the driver.
    inline constexpr char ONNX_RUNTIME_DIR[] = "plugins/dsinfer/inferencedrivers/onnx/runtime";

    /// The subdirectory of ONNX_RUNTIME_DIR that holds the CUDA flavor of the payload. Its DLLs
    /// have the same names as the DLLs of the default flavor, so the two flavors cannot share a
    /// directory.
    inline constexpr char CUDA_RUNTIME_SUBDIR[] = "cuda";

    /// The directory of the wolf language packages through which voicebank dependencies resolve.
    inline constexpr char LANGUAGE_PACKAGES_DIR[] = "wolf/packages";

}

#endif // DEPLOYLAYOUT_H
