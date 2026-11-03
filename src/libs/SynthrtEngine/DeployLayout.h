#ifndef DEPLOYLAYOUT_H
#define DEPLOYLAYOUT_H

#include <string_view>

/// Deployment paths relative to the plugin root, shared with CMake and Windows packaging.
/// Keep each definition on one line as: inline constexpr char NAME[] = "value";
namespace lite::synthrt::layout {

    /// Space-separated library directory names under plugins/.
    inline constexpr char PLUGIN_LIBRARIES[] = "dsinfer wolf otter";

    inline constexpr char DSINFER_INFERENCE_DIR[] = "plugins/dsinfer/inferenceinterpreters";

    inline constexpr char WOLF_INFERENCE_DIR[] = "plugins/wolf/inferenceinterpreters";

    inline constexpr char OTTER_INFERENCE_DIR[] = "plugins/otter/inferenceinterpreters";

    inline constexpr char DSINFER_SINGER_DIR[] = "plugins/dsinfer/singerproviders";

    inline constexpr char WOLF_LINGUIST_DIR[] = "plugins/wolf/linguistproviders";

    inline constexpr char INFERENCE_DRIVER_DIR[] = "plugins/dsinfer/inferencedrivers";

    inline constexpr char ONNX_RUNTIME_DIR[] = "plugins/dsinfer/inferencedrivers/onnx/runtime";

    /// Separate CUDA from the default payload because their DLL names overlap.
    inline constexpr char CUDA_RUNTIME_SUBDIR[] = "cuda";

    inline constexpr char LANGUAGE_PACKAGES_DIR[] = "wolf/packages";

    static_assert(std::string_view(ONNX_RUNTIME_DIR).starts_with(INFERENCE_DRIVER_DIR),
                  "ONNX_RUNTIME_DIR must lie below INFERENCE_DRIVER_DIR");

}

#endif // DEPLOYLAYOUT_H
