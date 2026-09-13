#include "SynthrtBootstrap.h"

#include <utility>

#include <dsinfer/Api/Drivers/Onnx/OnnxDriverApi.h>

#include <otter/Analysis/AnalysisContrib.h>
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

    srt::Expected<std::unique_ptr<Bootstrap>>
        Bootstrap::create(const fs::path &pluginRoot, const std::vector<fs::path> &packagePaths,
                          const fs::path &runtimePath, Backend backend, int deviceIndex) {
        // Named once so that a linker that drops unreferenced libraries, as ELF linkers do under
        // --as-needed and the MSVC linker does for every import library, keeps the two whose
        // static initializers register the linguist and analysis categories. Before the unit is
        // constructed, since a unit reads the registry once, when it is built.
        wolf::linkLinguistCategory();
        otter::linkAnalysisCategory();

        std::unique_ptr<Bootstrap> result(new Bootstrap());
        auto &impl = *result->_impl;

        impl.unit.setPackagePaths(packagePaths);

        // One search path per category. The refactor line named a separate plugin interface for
        // every interpreter kind; here the category is the unit of discovery, which is why adding
        // a domain adds one line rather than five.
        // Every library layered on synthrt installs its plugins under plugins/<library>/<category>,
        // so one root holds all of them and a category lists one directory per library.
        const fs::path inference[] = {pluginRoot / "plugins/dsinfer/inferenceinterpreters",
                                      pluginRoot / "plugins/wolf/inferenceinterpreters"};
        const fs::path singer[] = {pluginRoot / "plugins/dsinfer/singerproviders"};
        const fs::path linguist[] = {pluginRoot / "plugins/wolf/linguistproviders"};
        const fs::path analysis[] = {pluginRoot / "plugins/otter/analysisproviders"};
        impl.unit.setPluginPaths("inference", inference);
        impl.unit.setPluginPaths("singer", singer);
        impl.unit.setPluginPaths(wolf::LINGUIST_CATEGORY, linguist);
        impl.unit.setPluginPaths(otter::ANALYSIS_CATEGORY, analysis);

        // The driver is a runtime service of the whole unit, so everything that runs a model --
        // synthesis, grapheme to phoneme, pitch analysis -- shares one ONNX Runtime. On the
        // refactor line the language domain needed an adapter to borrow the inference driver;
        // here there is nothing to borrow, because there was only ever one.
        const fs::path driverPaths[] = {pluginRoot / "plugins/dsinfer/inferencedrivers"};
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
