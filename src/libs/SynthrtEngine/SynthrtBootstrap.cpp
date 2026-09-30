#include "SynthrtBootstrap.h"

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
                case Backend::DirectMl:
                    return OnnxApi::ExecutionProvider::DML;
                case Backend::CoreMl:
                    return OnnxApi::ExecutionProvider::CoreML;
                case Backend::Cpu:
                    break;
            }
            return OnnxApi::ExecutionProvider::CPU;
        }

    }

    Backend backendFromName(const std::string &name) {
        if (name == "CUDA") {
            return Backend::Cuda;
        }
        if (name == "DirectML") {
            return Backend::DirectMl;
        }
        if (name == "CoreML") {
            return Backend::CoreMl;
        }
        return Backend::Cpu;
    }

    class Bootstrap::Impl {
    public:
        // The factory owns the loaded driver plugin and must outlive the driver it created, which
        // the unit owns. Declared first so it is destroyed last.
        ds::InferenceDriverFactory drivers;
        srt::SynthUnit unit;
        bool driver = false;
    };

    std::vector<PluginCategory> pluginCategories(const fs::path &pluginRoot) {
        const auto plugins = pluginRoot / "plugins";
        return {
            {srt::InferenceCategory::NAME,
             {plugins / "dsinfer/inferenceinterpreters", plugins / "wolf/inferenceinterpreters",
              plugins / "otter/inferenceinterpreters"}},
            {srt::SingerCategory::NAME,     {plugins / "dsinfer/singerproviders"}},
            {wolf::LINGUIST_CATEGORY,       {plugins / "wolf/linguistproviders"}},
        };
    }

    fs::path driverDirectory(const fs::path &pluginRoot) {
        return pluginRoot / "plugins/dsinfer/inferencedrivers";
    }

    srt::Expected<std::unique_ptr<Bootstrap>>
        Bootstrap::create(const fs::path &pluginRoot, const std::vector<fs::path> &packagePaths,
                          const fs::path &runtimePath, Backend backend, int deviceIndex) {
        // Named once so that a linker that drops unreferenced libraries, as ELF linkers do under
        // --as-needed and the MSVC linker does for every import library, keeps the library whose
        // static initializer registers the linguist category. Called before the unit is
        // constructed, because a unit reads the registry once, at construction.
        wolf::linkLinguistCategory();

        std::unique_ptr<Bootstrap> result(new Bootstrap());
        auto &impl = *result->_impl;

        impl.unit.setPackagePaths(packagePaths);

        // One call per category, because each call replaces the plugin list of that category.
        for (const auto &[category, directories] : pluginCategories(pluginRoot)) {
            impl.unit.setPluginPaths(category, directories);
        }

        // The driver is a runtime service of the whole unit, so everything that runs a model --
        // synthesis, grapheme to phoneme, pitch analysis -- shares one ONNX Runtime. On the
        // refactor line the language domain needed an adapter to borrow the inference driver;
        // here there is nothing to borrow, because there was only ever one.
        const fs::path driverPaths[] = {driverDirectory(pluginRoot)};
        impl.drivers.setPluginPaths(driverPaths);
        auto *loader = impl.drivers.find(OnnxApi::API_NAME);
        if (loader == nullptr) {
            // Not fatal. Packages still load and the editor still lists voicebanks; only running
            // a model is unavailable, and saying so beats refusing to start.
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
        // Named by the host rather than looked up by the driver, so which copy of ONNX Runtime is
        // loaded is a deployment decision and not a search order.
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
