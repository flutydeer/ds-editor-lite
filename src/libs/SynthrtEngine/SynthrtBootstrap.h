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

    /// The plugin directories of one contribution category.
    struct PluginCategory {
        std::string category;
        std::vector<fs::path> directories;
    };

    /// Returns every category that the editor registers, with its plugin directories below
    /// \a pluginRoot.
    ///
    /// Each library layered on synthrt installs its plugins under plugins/<library>/<category>,
    /// so a category lists one directory per contributing library. All three libraries contribute
    /// to the inference category: dsinfer with its models, wolf with its language conversions and
    /// otter with its analyzers. This function is the single source of the list. Bootstrap
    /// registers the list, and the settings page displays the paths from it rather than deriving
    /// them again.
    std::vector<PluginCategory> pluginCategories(const fs::path &pluginRoot);

    /// Returns the inference driver directory below \a pluginRoot. Drivers are runtime services of
    /// the unit rather than contributions, so a separate factory locates them.
    fs::path driverDirectory(const fs::path &pluginRoot);

    /// Setup of a unit that is required before a package can be opened.
    ///
    /// This class replaces the Runtime, the PluginFactory and the two separate driver setups of
    /// the refactor branch. It is the only place that configures the plugin search path of every
    /// category that the editor uses and the ONNX driver. The driver is registered once on the
    /// unit rather than per module, so the language and analysis domains share one runtime and
    /// need no dedicated adapter to borrow it.
    class Bootstrap {
    public:
        /// Builds a unit with the categories that the editor uses and the ONNX driver registered.
        ///
        /// \a pluginRoot is the directory that contains the installed plugin trees. \a runtimePath
        /// is the deployment directory of ONNX Runtime, which the host specifies so that the
        /// driver does not search for it.
        ///
        /// \a packagePaths are the package search paths, which differ from the directories that a
        /// voicebank scan traverses. A scan opens the packages it finds by path, whereas a
        /// dependency is resolved through the package search paths. Without them, a voicebank that
        /// references a language package fails to load with an error about the reference rather
        /// than about the search path, which obscures the cause.
        static srt::Expected<std::unique_ptr<Bootstrap>>
            create(const fs::path &pluginRoot, const std::vector<fs::path> &packagePaths,
                   const fs::path &runtimePath, Backend backend, int deviceIndex);

        ~Bootstrap();

        srt::SynthUnit &unit();

        /// Returns whether a model can be opened. Returns false if no driver plugin was found; in
        /// that case packages still load and only inference is unavailable.
        bool hasDriver() const;

    private:
        Bootstrap();

        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // SYNTHRTBOOTSTRAP_H
