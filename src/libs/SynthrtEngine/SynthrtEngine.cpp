#include "SynthrtEngine.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <tuple>
#include <mutex>
#include <shared_mutex>
#include <utility>

#include <QDebug>

#include <lite/Core/SingletonRegistry.h>
#include <lite/Support/VersionUtils.h>

#include <otter/Analysis/AnalysisContrib.h>

#include <stdcorelib/system.h>

#if defined(Q_OS_MAC)
#    include <lite/Support/MacOSUtils.h>
#endif

namespace fs = std::filesystem;

namespace {

    std::vector<fs::path> toPaths(const QStringList &values) {
        std::vector<fs::path> result;
        result.reserve(values.size());
        for (const auto &value : values) {
            if (!value.isEmpty()) {
                result.push_back(fs::path(value.toStdString()));
            }
        }
        return result;
    }

}

namespace {

    /// The language layer's address for a singer, version and all.
    ///
    /// The version is carried rather than left out: the specification allows two versions of one
    /// package to be loaded at once, and a module reference cannot distinguish them. Asking
    /// without it is answered only while exactly one version is installed, and silently answers
    /// nothing the day someone installs both.
    lite::synthrt::LanguageBridge::Singer singerOf(const SingerIdentifier &identifier) {
        const auto [packageId, contributionId] = identifier.contribution();
        return {packageId, contributionId, VersionUtils::qt_to_stdc(identifier.packageVersion)};
    }

}

class SynthrtEngine::Impl {
public:
    /// Guards the engine's state against a shutdown running under it.
    ///
    /// Shared while anything is using the unit, exclusive while it is replaced or torn down. The
    /// refactor line called this the runtime lifecycle lock and it is here for the same reason: a
    /// package handle, a pipeline and a conversion all borrow from the unit, and the unit going
    /// away under one of them is not something they can be asked to tolerate.
    mutable std::shared_mutex lifecycle;

    std::unique_ptr<lite::synthrt::Bootstrap> bootstrap;
    std::unique_ptr<lite::synthrt::LanguageBridge> language;

    /// The packages the last scan loaded. Held so they stay loaded; released on refresh.
    std::vector<srt::PackageHandle> packages;
    std::vector<lite::synthrt::SingerEntry> catalog;

    /// The pipelines currently held by somebody, keyed the way the editor names a singer:
    /// package, version and contribution. The version is part of the key for the same reason
    /// singerOf() carries it, since two versions of one package may be loaded at once and a
    /// pipeline built from one must not be handed out for the other. Weak, because the holders
    /// own the pipelines: this only lets two holders share one.
    using PipelineKey = std::tuple<std::string, std::string, std::string>;
    std::map<PipelineKey, std::weak_ptr<lite::synthrt::SingerPipeline>> pipelines;

    static PipelineKey pipelineKey(const SingerIdentifier &identifier) {
        const auto [packageId, contributionId] = identifier.contribution();
        return {packageId, VersionUtils::qt_to_stdc(identifier.packageVersion).toString(),
                contributionId};
    }

    /// Bumped whenever the packages a pipeline borrows from are released, so that a caller
    /// holding a pipeline can tell whether it still means anything.
    std::atomic_uint64_t generation = 0;

    std::atomic_bool initialized = false;
    std::atomic_bool initializationDone = false;
    std::atomic_bool aboutToQuit = false;

    mutable std::mutex doneMutex;
    mutable std::condition_variable done;

    /// Finds a singer in the catalogue. The caller holds the lifecycle lock.
    const lite::synthrt::SingerEntry *find(const SingerIdentifier &identifier) const {
        const auto [packageId, contributionId] = identifier.contribution();
        for (const auto &entry : catalog) {
            if (entry.packageId == packageId && entry.contributionId == contributionId) {
                return &entry;
            }
        }
        return nullptr;
    }

    void releasePackages() {
        // A pipeline somebody still holds keeps its own handle on its package, so releasing the
        // engine's handles here leaves it whole; the table of shared pipelines is dropped so that
        // the next request builds from the fresh scan rather than joining an old holder.
        pipelines.clear();
        for (auto &package : packages) {
            package.reset();
        }
        generation.fetch_add(1, std::memory_order_release);
        packages.clear();
    }
};

SynthrtEngine &SynthrtEngine::instance() {
    auto *engine = SingletonRegistry::instance<SynthrtEngine>();
    if (!engine) {
        qFatal("SynthrtEngine::instance() requires an owner to register it "
               "(e.g. the app's AppContext)");
    }
    return *engine;
}

SynthrtEngine::SynthrtEngine(QObject *parent) : QObject(parent), _impl(std::make_unique<Impl>()) {
}

SynthrtEngine::~SynthrtEngine() {
    shutdown();
}

fs::path SynthrtEngine::defaultPluginRoot() {
    // The directory that *holds* the plugin tree, not the tree itself: each of the three
    // packages installs under plugins/<library>/<category> below it, and Bootstrap appends the
    // rest, so this has to be their common parent or every path below it is wrong by one level.
#if defined(Q_OS_MAC)
    return MacOSUtils::getMainBundlePath() / "Contents/PlugIns";
#elif defined(Q_OS_WIN)
    return stdc::system::application_directory();
#else
    return stdc::system::application_directory().parent_path() / "lib";
#endif
}

fs::path SynthrtEngine::defaultRuntimePath() {
    // Beside the driver plugin, where the build deploys it. Named rather than searched for: which
    // copy of ONNX Runtime is loaded is a deployment decision, and letting the driver look for one
    // is how a machine ends up running a different copy from the one that shipped.
    return defaultPluginRoot() / "plugins/dsinfer/inferencedrivers/onnx/runtime";
}

bool SynthrtEngine::initialize(const QStringList &voicebankPaths, const QStringList &packagePaths,
                               const QString &ep, int deviceIndex, const fs::path &pluginRoot,
                               const fs::path &runtimePath) {
    const auto announce = [this](bool ok) {
        _impl->initialized.store(ok, std::memory_order_release);
        {
            std::lock_guard guard(_impl->doneMutex);
            _impl->initializationDone.store(true, std::memory_order_release);
        }
        _impl->done.notify_all();
        return ok;
    };

    std::unique_lock lock(_impl->lifecycle);
    if (_impl->aboutToQuit.load(std::memory_order_acquire)) {
        return announce(false);
    }

    auto searched = toPaths(packagePaths);
    for (auto &path : toPaths(voicebankPaths)) {
        // A voicebank directory is also a place to look for a dependency, since a language package
        // may well be installed beside the voicebanks that use it.
        searched.push_back(std::move(path));
    }

    auto booted = lite::synthrt::Bootstrap::create(
        pluginRoot, searched, runtimePath, lite::synthrt::backendFromName(ep.toStdString()),
        deviceIndex);
    if (!booted) {
        qCritical().noquote() << "SynthrtEngine: the unit could not be built:"
                              << QString::fromStdString(booted.error().toString());
        return announce(false);
    }
    _impl->bootstrap = booted.take();
    _impl->language = std::make_unique<lite::synthrt::LanguageBridge>(_impl->bootstrap->unit());

    if (!_impl->bootstrap->hasDriver()) {
        // Degraded rather than failed: voicebanks still list and a project still opens, and a
        // person is far better served by being told than by the editor refusing to start.
        qWarning() << "SynthrtEngine: no ONNX driver was found; inference is unavailable";
    }

    std::vector<lite::synthrt::PackageProblem> problems;
    _impl->catalog = lite::synthrt::scan(_impl->bootstrap->unit(), toPaths(voicebankPaths),
                                         _impl->packages, problems);
    for (const auto &problem : problems) {
        qWarning().noquote() << "SynthrtEngine: could not open"
                             << QString::fromStdString(problem.path.string()) << ":"
                             << QString::fromStdString(problem.reason);
    }
    _impl->language->refresh();
    return announce(true);
}

bool SynthrtEngine::initialized() const noexcept {
    return _impl->initialized.load(std::memory_order_acquire);
}

bool SynthrtEngine::initializationDone() const noexcept {
    return _impl->initializationDone.load(std::memory_order_acquire);
}

bool SynthrtEngine::waitForInitialization(int timeoutMs) const {
    std::unique_lock guard(_impl->doneMutex);
    return _impl->done.wait_for(guard, std::chrono::milliseconds(timeoutMs),
                                [this] { return initializationDone(); });
}

bool SynthrtEngine::hasInferenceBackend() const noexcept {
    std::shared_lock lock(_impl->lifecycle);
    return _impl->bootstrap && _impl->bootstrap->hasDriver();
}

bool SynthrtEngine::isAboutToQuit() const noexcept {
    return _impl->aboutToQuit.load(std::memory_order_acquire);
}

void SynthrtEngine::shutdown() noexcept {
    _impl->aboutToQuit.store(true, std::memory_order_release);
    std::unique_lock lock(_impl->lifecycle);
    _impl->initialized.store(false, std::memory_order_release);

    _impl->releasePackages();
    // The language session owns executives of its own and must go before the unit it borrows.
    _impl->language.reset();
    _impl->catalog.clear();
    _impl->bootstrap.reset();
}

srt::Expected<std::vector<lite::synthrt::SingerEntry>>
    SynthrtEngine::refreshVoicebanks(const std::vector<fs::path> &searchPaths,
                                     std::vector<lite::synthrt::PackageProblem> *problems) {
    std::unique_lock lock(_impl->lifecycle);
    if (!_impl->bootstrap) {
        return srt::Error(srt::Error::InvalidArgument, "the engine is not initialized");
    }

    _impl->releasePackages();

    std::vector<lite::synthrt::PackageProblem> failures;
    _impl->catalog =
        lite::synthrt::scan(_impl->bootstrap->unit(), searchPaths, _impl->packages, failures);
    for (const auto &problem : failures) {
        qWarning().noquote() << "SynthrtEngine: could not open"
                             << QString::fromStdString(problem.path.string()) << ":"
                             << QString::fromStdString(problem.reason);
    }
    if (problems != nullptr) {
        *problems = std::move(failures);
    }
    if (_impl->language) {
        _impl->language->refresh();
    }
    return _impl->catalog;
}

std::uint64_t SynthrtEngine::catalogGeneration() const noexcept {
    return _impl->generation.load(std::memory_order_acquire);
}

std::vector<lite::synthrt::SingerEntry> SynthrtEngine::singers() const {
    std::shared_lock lock(_impl->lifecycle);
    return _impl->catalog;
}

srt::Expected<lite::synthrt::SingerEntry>
    SynthrtEngine::singer(const SingerIdentifier &identifier) const {
    std::shared_lock lock(_impl->lifecycle);
    if (const auto *entry = _impl->find(identifier)) {
        return *entry;
    }
    return srt::Error(srt::Error::FileNotFound,
                      "no loaded voicebank holds the singer " + identifier.singerId.toStdString());
}

srt::Expected<SingerIdentifier> SynthrtEngine::findSinger(const QString &singerId) const {
    std::shared_lock lock(_impl->lifecycle);
    const auto wanted = singerId.toStdString();
    for (const auto &entry : _impl->catalog) {
        if (entry.contributionId == wanted) {
            SingerIdentifier identifier;
            identifier.singerId = singerId;
            identifier.packageId = QString::fromStdString(entry.packageId);
            identifier.packageVersion = QVersionNumber::fromString(
                QString::fromStdString(entry.packageVersion.toString()));
            return identifier;
        }
    }
    return srt::Error(srt::Error::FileNotFound,
                      "no loaded voicebank holds a singer called " + wanted);
}

fs::path SynthrtEngine::packageDirectory(const SingerIdentifier &identifier) const {
    std::shared_lock lock(_impl->lifecycle);
    const auto *entry = _impl->find(identifier);
    return entry ? entry->packagePath : fs::path();
}

srt::Expected<std::shared_ptr<lite::synthrt::SingerPipeline>>
    SynthrtEngine::pipelineFor(const SingerIdentifier &identifier) {
    std::unique_lock lock(_impl->lifecycle);
    if (!_impl->bootstrap) {
        return srt::Error(srt::Error::InvalidArgument, "the engine is not initialized");
    }
    const auto key = Impl::pipelineKey(identifier);
    if (const auto it = _impl->pipelines.find(key); it != _impl->pipelines.end()) {
        if (auto held = it->second.lock()) {
            return held;
        }
        _impl->pipelines.erase(it);
    }
    const auto &[packageId, packageVersion, contributionId] = key;

    // The declaration is reached through the package handle rather than kept in the catalogue: a
    // ContribSpec belongs to its package, and the pipeline takes the handle along so that the
    // declaration outlives any refresh for as long as the pipeline does.
    srt::ContribSpec *spec = nullptr;
    const srt::PackageHandle *owner = nullptr;
    for (auto &package : _impl->packages) {
        if (package.id() == packageId && package.version().toString() == packageVersion) {
            spec = package.contribution("singer", contributionId);
            if (spec != nullptr) {
                owner = &package;
                break;
            }
        }
    }
    if (spec == nullptr) {
        return srt::Error(srt::Error::FileNotFound,
                          "no loaded voicebank holds the singer " + contributionId);
    }

    auto built = lite::synthrt::SingerPipeline::create(*owner, *spec->as<srt::SingerSpec>());
    if (!built) {
        return built.takeError();
    }
    std::shared_ptr<lite::synthrt::SingerPipeline> pipeline(built.take().release());
    _impl->pipelines[key] = pipeline;
    return pipeline;
}

std::vector<lite::synthrt::AnalyzerEntry>
    SynthrtEngine::analyzers(const QString &interfaceName) const {
    std::shared_lock lock(_impl->lifecycle);
    auto found = lite::synthrt::analyzersOf(_impl->packages);
    if (interfaceName.isEmpty()) {
        return found;
    }
    const auto wanted = interfaceName.toStdString();
    std::vector<lite::synthrt::AnalyzerEntry> result;
    for (auto &entry : found) {
        if (entry.interfaceName == wanted) {
            result.push_back(std::move(entry));
        }
    }
    return result;
}

srt::ContribSpec *SynthrtEngine::findAnalyzer(const QString &reference,
                                              const srt::PackageHandle **package) const {
    // The caller holds the lifecycle lock.
    const auto text = reference.toStdString();
    const auto separator = text.find(':');
    if (separator == std::string::npos) {
        return nullptr;
    }
    const auto packageId = text.substr(0, separator);
    const auto rest = text.substr(separator + 1);
    const auto slash = rest.find('/');
    if (slash == std::string::npos || rest.substr(0, slash) != otter::ANALYSIS_CATEGORY) {
        return nullptr;
    }
    const auto contributionId = rest.substr(slash + 1);
    for (const auto &candidate : _impl->packages) {
        if (candidate.id() == packageId) {
            if (auto *spec = candidate.contribution(otter::ANALYSIS_CATEGORY, contributionId)) {
                if (package != nullptr) {
                    *package = &candidate;
                }
                return spec;
            }
        }
    }
    return nullptr;
}

srt::Expected<SynthrtEngine::AnalyzerLease>
    SynthrtEngine::createAnalyzer(const QString &reference) {
    std::shared_lock lock(_impl->lifecycle);
    const srt::PackageHandle *package = nullptr;
    auto *spec = findAnalyzer(reference, &package);
    if (spec == nullptr) {
        return srt::Error(srt::Error::FileNotFound,
                          "no installed package holds the analyser "
                              + reference.toStdString());
    }
    // Which extension to ask for depends on the contract, since each is keyed on its own
    // executive type. A contract this build does not know is refused rather than guessed at.
    auto *analysis = spec->as<otter::AnalysisSpec>();
    srt::ContribSpecExtension *extension = nullptr;
    if (spec->interface() == otter::Api::F0::L1::API_INTERFACE) {
        extension =
            srt::ContribSpecExtension::findFromSpec<otter::Api::F0::L1::F0Executive>(*analysis);
    } else if (spec->interface() == otter::Api::Note::L1::API_INTERFACE) {
        extension =
            srt::ContribSpecExtension::findFromSpec<otter::Api::Note::L1::NoteExecutive>(*analysis);
    }
    if (extension == nullptr) {
        return srt::Error(srt::Error::FeatureNotSupported,
                          "this analyser declares a contract no installed provider serves");
    }

    auto created = [&]() -> srt::Expected<std::unique_ptr<otter::AnalysisExecutive>> {
        auto *analysisExtension = extension->as<otter::AnalysisExtension>();
        if (spec->interface() == otter::Api::F0::L1::API_INTERFACE) {
            otter::Api::F0::L1::F0RuntimeOptions options(spec->variant());
            return analysisExtension->createAnalyzer(options);
        }
        otter::Api::Note::L1::NoteRuntimeOptions options(spec->variant());
        return analysisExtension->createAnalyzer(options);
    }();
    if (!created) {
        return created.takeError();
    }
    AnalyzerLease lease;
    lease.package = *package;
    lease.spec = spec;
    lease.executive = created.take();
    return lease;
}

QStringList SynthrtEngine::languagesOf(const SingerIdentifier &identifier) const {
    std::shared_lock lock(_impl->lifecycle);
    QStringList result;
    if (!_impl->language) {
        return result;
    }
    for (const auto &language : _impl->language->languagesOf(singerOf(identifier))) {
        result << QString::fromStdString(language);
    }
    return result;
}

bool SynthrtEngine::canConvert(const SingerIdentifier &identifier, const QString &language) const {
    std::shared_lock lock(_impl->lifecycle);
    if (!_impl->language) {
        return false;
    }
    return _impl->language->canConvert(singerOf(identifier), language.toStdString());
}

srt::Expected<std::vector<lite::synthrt::LanguageBridge::Result>>
    SynthrtEngine::convert(const SingerIdentifier &identifier, const QString &language,
                           const std::vector<lite::synthrt::LanguageBridge::Word> &words,
                           lite::synthrt::LanguageBridge::Depth depth) const {
    std::shared_lock lock(_impl->lifecycle);
    if (!_impl->language) {
        return srt::Error(srt::Error::InvalidArgument, "the engine is not initialized");
    }
    return _impl->language->convert(singerOf(identifier), language.toStdString(), words,
                                    depth);
}

void SynthrtEngine::cancelConversions() {
    std::shared_lock lock(_impl->lifecycle);
    if (_impl->language) {
        _impl->language->cancel();
    }
}

void SynthrtEngine::setSingerPhonemes(const SingerIdentifier &identifier,
                                      std::vector<std::string> phonemes) {
    std::shared_lock lock(_impl->lifecycle);
    if (!_impl->language) {
        return;
    }
    _impl->language->setSingerPhonemes(singerOf(identifier), std::move(phonemes));
}

void SynthrtEngine::setReservedMarkers(std::vector<std::string> markers) {
    std::shared_lock lock(_impl->lifecycle);
    if (_impl->language) {
        _impl->language->setReservedMarkers(std::move(markers));
    }
}

std::vector<std::string> SynthrtEngine::reservedMarkers() const {
    std::shared_lock lock(_impl->lifecycle);
    return _impl->language ? _impl->language->reservedMarkers() : std::vector<std::string>();
}

srt::SynthUnit &SynthrtEngine::unit() {
    return _impl->bootstrap->unit();
}
