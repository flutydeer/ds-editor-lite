#ifndef SYNTHRTBOOTSTRAP_H
#define SYNTHRTBOOTSTRAP_H

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <synthrt/Core/SynthUnit.h>
#include <synthrt/Support/Expected.h>

#include <dsinfer/Inference/InferenceDriverFactory.h>

namespace lite::synthrt {

    namespace fs = std::filesystem;

    /// Which backend models run on.
    enum class Backend {
        Cpu,
        Cuda,
        DirectMl,
        CoreMl,
    };

    /// Reads a backend from the name the editor's settings store.
    ///
    /// An unknown name reads as Cpu rather than failing: a settings file written by a newer build,
    /// or on another platform, should not stop the editor starting.
    Backend backendFromName(const std::string &name);

    /// Everything a unit needs before a package can be opened.
    ///
    /// This replaces the refactor line's Runtime plus its PluginFactory plus its two separate
    /// driver setups. Three things happen here and nowhere else: the plugin search path of every
    /// category the editor uses, the ONNX driver, and the fact that the driver is registered once
    /// on the unit rather than per module. The last is why the language and analysis domains do
    /// not each load a runtime of their own, which on the refactor line took a dedicated adapter.
    class Bootstrap {
    public:
        /// Builds a unit with the categories the editor uses and the ONNX driver registered.
        ///
        /// \a pluginRoot is the directory holding the installed plugin trees; \a runtimePath is
        /// where ONNX Runtime was deployed, which the host names rather than the driver guessing.
        ///
        /// \a packagePaths is where packages are searched for, and it is not the same thing as
        /// the directories a voicebank scan walks. A scan opens what it finds by path; a
        /// dependency is resolved through these. A voicebank that names a language package would
        /// fail to load without them, and it would fail talking about the reference rather than
        /// about a search path, which is a long way from the cause.
        static srt::Expected<std::unique_ptr<Bootstrap>>
            create(const fs::path &pluginRoot, const std::vector<fs::path> &packagePaths,
                   const fs::path &runtimePath, Backend backend, int deviceIndex);

        ~Bootstrap();

        srt::SynthUnit &unit();

        /// Whether a model can actually be opened. False when no driver plugin was found, in
        /// which case packages still load and only inference is unavailable.
        bool hasDriver() const;

    private:
        Bootstrap();

        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // SYNTHRTBOOTSTRAP_H
