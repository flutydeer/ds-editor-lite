#ifndef SYNTHRTBOOTSTRAP_H
#define SYNTHRTBOOTSTRAP_H

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <synthrt/Core/SynthUnit.h>
#include <synthrt/Support/Expected.h>

#include <dsinfer/Inference/InferenceDriverFactory.h>

#include "ExecutionBackend.h"

namespace lite::synthrt {

    namespace fs = std::filesystem;

    struct PluginCategory {
        std::string category;
        std::vector<fs::path> directories;
    };

    /// Category plugin paths shared by bootstrap and the settings display.
    std::vector<PluginCategory> pluginCategories(const fs::path &pluginRoot);

    /// Driver lookup is separate from contribution-category lookup.
    fs::path driverDirectory(const fs::path &pluginRoot);

    /// Register contribution categories, plugin paths, and the shared ONNX driver.
    class Bootstrap {
    public:
        /// Create a unit using the supplied plugin and runtime directories.
        /// packagePaths resolves dependencies; it is distinct from voicebank scan roots.
        static srt::Expected<std::unique_ptr<Bootstrap>>
            create(const fs::path &pluginRoot, const std::vector<fs::path> &packagePaths,
                   const fs::path &runtimePath, Backend backend, int deviceIndex);

        ~Bootstrap();

        srt::SynthUnit &unit();

        /// False when no ONNX driver is available; package metadata may still load.
        bool hasDriver() const;

    private:
        Bootstrap();

        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // SYNTHRTBOOTSTRAP_H
