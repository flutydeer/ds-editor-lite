#include "SynthrtBootstrap.h"

#include "DeployLayout.h"

#include <utility>

#include <synthrt/SVS/InferenceContrib.h>
#include <synthrt/SVS/SingerContrib.h>

#include <dsinfer/Api/Drivers/Onnx/OnnxDriverApi.h>

#include <wolf/Linguist/LinguistContrib.h>

namespace lite::synthrt {

    namespace {

        namespace OnnxApi = ds::Api::Onnx;

        OnnxApi::ExecutionProvider providerFor(Backend backend) {
            switch (backend) {
                case Backend::Cuda:
                    return OnnxApi::ExecutionProvider::CUDA;
                case Backend::DirectML:
                    return OnnxApi::ExecutionProvider::DML;
                case Backend::CoreML:
                    return OnnxApi::ExecutionProvider::CoreML;
                case Backend::Cpu:
                    break;
            }
            return OnnxApi::ExecutionProvider::CPU;
        }

    }

    class Bootstrap::Impl {
    public:
        // The factory owns the loaded driver plugin and must outlive the driver that it created,
        // which the unit owns. Declared first so that it is destroyed last.
        ds::InferenceDriverFactory drivers;
        srt::SynthUnit unit;
        bool driver = false;
    };

    std::vector<PluginCategory> pluginCategories(const fs::path &pluginRoot) {
        // The directories are defined in DeployLayout.h, which the deployment also parses.
        return {
            {srt::InferenceCategory::NAME,
             {pluginRoot / layout::DSINFER_INFERENCE_DIR, pluginRoot / layout::WOLF_INFERENCE_DIR,
              pluginRoot / layout::OTTER_INFERENCE_DIR}},
            {srt::SingerCategory::NAME, {pluginRoot / layout::DSINFER_SINGER_DIR}},
            {wolf::LINGUIST_CATEGORY, {pluginRoot / layout::WOLF_LINGUIST_DIR}},
        };
    }

    fs::path driverDirectory(const fs::path &pluginRoot) {
        return pluginRoot / layout::INFERENCE_DRIVER_DIR;
    }

    srt::Expected<std::unique_ptr<Bootstrap>>
        Bootstrap::create(const fs::path &pluginRoot, const std::vector<fs::path> &packagePaths,
                          const fs::path &runtimePath, Backend backend, int deviceIndex) {
        // Referenced so that a linker that drops unreferenced libraries, as ELF linkers do under
        // --as-needed and the MSVC linker does for every import library, keeps the library whose
        // static initializer registers the linguist category. Called before the unit is
        // constructed, because a unit reads the registry only once, at construction.
        wolf::linkLinguistCategory();

        std::unique_ptr<Bootstrap> result(new Bootstrap());
        auto &impl = *result->_impl;

        impl.unit.setPackagePaths(packagePaths);

        // One call per category, because each call replaces the plugin list of that category.
        for (const auto &[category, directories] : pluginCategories(pluginRoot)) {
            impl.unit.setPluginPaths(category, directories);
        }

        // The driver is a runtime service of the whole unit, so synthesis, grapheme-to-phoneme
        // conversion and pitch analysis share one ONNX Runtime. The language domain therefore
        // needs no adapter to borrow the inference driver, unlike on the refactor branch.
        const fs::path driverPaths[] = {driverDirectory(pluginRoot)};
        impl.drivers.setPluginPaths(driverPaths);
        auto *loader = impl.drivers.find(OnnxApi::API_NAME);
        if (loader == nullptr) {
            // Not fatal. Packages still load and the editor still lists voicebanks. Only model
            // execution is unavailable, which is preferable to a failed startup.
            return result;
        }
        auto created = impl.drivers.create(loader);
        if (!created) {
            return created.takeError().withContext("the ONNX driver plugin could not be created");
        }
        auto driver = created.take();

        OnnxApi::DriverInitArgs args;
        args.ep = providerFor(backend);
        args.deviceIndex = deviceIndex;
        // Specified by the host rather than searched for by the driver, so the deployment, not a
        // search order, determines which copy of ONNX Runtime is loaded.
        args.runtimePath = runtimePath;
        if (auto initialized = driver->initialize(args); !initialized) {
            return initialized.takeError().withContext(
                "the ONNX driver could not be initialized");
        }
        if (auto added = impl.unit.addRuntimeService(std::move(driver)); !added) {
            return added.takeError().withContext("the ONNX driver could not be registered");
        }
        impl.driver = true;
        return result;
    }

    Bootstrap::Bootstrap() : _impl(std::make_unique<Impl>()) {
    }

    Bootstrap::~Bootstrap() = default;

    srt::SynthUnit &Bootstrap::unit() {
        return _impl->unit;
    }

    bool Bootstrap::hasDriver() const {
        return _impl->driver;
    }

}
