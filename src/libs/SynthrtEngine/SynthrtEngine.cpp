#include "SynthrtEngine.h"

#include "AnalysisContracts.h"
#include "SingerPipeline.h"
#include "SynthrtBootstrap.h"
#include "DeployLayout.h"
#include "ReservedPhonemes.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <optional>
#include <set>
#include <tuple>
#include <mutex>
#include <shared_mutex>
#include <utility>

#include <QDebug>
#include <QDir>

#include <lite/Core/SingletonRegistry.h>
#include <lite/Support/StringUtils.h>
#include <lite/Support/VersionUtils.h>

#include <synthrt/SVS/InferenceContrib.h>
#include <synthrt/SVS/SingerContrib.h>

#include <otter/Analysis/AnalysisExecutive.h>
#include <otter/Api/F0/1/F0ApiL1.h>
#include <otter/Api/Note/1/NoteApiL1.h>

#include <stdcorelib/system.h>

#if defined(Q_OS_MAC)
#    include <lite/Support/MacOSUtils.h>
#endif

namespace fs = std::filesystem;

QString lite::synthrt::f0Contract() {
    return QString::fromLatin1(otter::Api::F0::L1::API_INTERFACE);
}

QString lite::synthrt::noteContract() {
    return QString::fromLatin1(otter::Api::Note::L1::API_INTERFACE);
}

namespace {

    std::vector<fs::path> toPaths(const QStringList &values) {
        std::vector<fs::path> result;
        result.reserve(values.size());
        for (const auto &value : values) {
            if (!value.isEmpty()) {
                result.push_back(StringUtils::qstr_to_path(value));
            }
        }
        return result;
    }

}

namespace {

    lite::synthrt::LanguageBridge::Singer singerOf(const SingerIdentifier &identifier) {
        const auto [packageId, contributionId] = identifier.contribution();
        return {packageId, contributionId, VersionUtils::qt_to_stdc(identifier.packageVersion)};
    }

}

class SynthrtEngine::Impl {
public:
    /// Shared during SDK use, exclusive during runtime replacement or shutdown.
    mutable std::shared_mutex lifecycle;

    std::shared_ptr<lite::synthrt::Bootstrap> bootstrap;
    fs::path pluginDirectory;
    fs::path runtimeDirectory;
    lite::synthrt::Backend backend = lite::synthrt::Backend::Cpu;
    int driverDeviceIndex = 0;
    std::vector<fs::path> dependencyPaths;

    std::vector<fs::path> searchPathsFor(const std::vector<fs::path> &voicebanks) const {
        auto paths = dependencyPaths;
        for (const auto &path : voicebanks) {
            std::error_code error;
            const auto dependencyRoot = fs::is_regular_file(path / "desc.json", error)
                                            ? path.parent_path()
                                            : path;
            if (std::find(paths.begin(), paths.end(), dependencyRoot) == paths.end())
                paths.push_back(dependencyRoot);
        }
        return paths;
    }
    std::unique_ptr<lite::synthrt::LanguageBridge> language;

    /// Package handles for the published catalog.
    std::vector<srt::PackageHandle> packages;
    std::vector<lite::synthrt::SingerEntry> catalog;

    /// Weak pipeline cache keyed by package, version, and contribution; callers own the pipelines.
    using PipelineKey = std::tuple<std::string, std::string, std::string>;
    std::map<PipelineKey, std::weak_ptr<lite::synthrt::SingerPipeline>> pipelines;

    static PipelineKey pipelineKey(const SingerIdentifier &identifier) {
        const auto [packageId, contributionId] = identifier.contribution();
        return {packageId, VersionUtils::qt_to_stdc(identifier.packageVersion).toString(),
                contributionId};
    }

    std::atomic_uint64_t generation = 0;

    std::atomic_bool initialized = false;
    std::atomic_bool initializationDone = false;
    std::atomic_bool aboutToQuit = false;

    mutable std::mutex doneMutex;
    mutable std::condition_variable done;

    /// Find an exact version, or the highest loaded version when unspecified. Requires the
    /// lifecycle lock.
    const lite::synthrt::SingerEntry *find(const SingerIdentifier &identifier) const {
        const auto [packageId, contributionId] = identifier.contribution();
        const auto wanted = identifier.packageVersion.isNull()
                                ? std::optional<stdc::VersionNumber>()
                                : VersionUtils::qt_to_stdc(identifier.packageVersion);
        const lite::synthrt::SingerEntry *best = nullptr;
        for (const auto &entry : catalog) {
            if (entry.packageId != packageId || entry.contributionId != contributionId) {
                continue;
            }
            if (wanted) {
                if (entry.packageVersion == *wanted) {
                    return &entry;
                }
                continue;
            }
            if (best == nullptr || best->packageVersion < entry.packageVersion) {
                best = &entry;
            }
        }
        return best;
    }

    /// Resolve the version or preserve an unmatched identifier. Requires the lifecycle lock.
    SingerIdentifier resolved(const SingerIdentifier &identifier) const {
        if (!identifier.packageVersion.isNull()) {
            return identifier;
        }
        auto result = identifier;
        if (const auto *entry = find(identifier)) {
            result.packageVersion = VersionUtils::stdc_to_qt(entry->packageVersion);
        }
        return result;
    }

    /// Write scan results to temporary containers for atomic publication.
    /// Associate exceptions with the search path that failed.
    bool scanInto(const std::vector<fs::path> &paths,
                  std::vector<lite::synthrt::SingerEntry> &entries,
                  std::vector<srt::PackageHandle> &opened,
                  std::vector<lite::synthrt::PackageProblem> &problems) {
        bool complete = true;
        for (const auto &path : paths) {
            try {
                auto scanned = lite::synthrt::scan(bootstrap->unit(), {path}, opened, problems);
                for (auto &entry : scanned) {
                    entries.push_back(std::move(entry));
                }
            } catch (const std::exception &e) {
                problems.push_back({path, std::string("the scan failed: ") + e.what()});
                complete = false;
                break;
            }
        }
        for (const auto &problem : problems) {
            qWarning().noquote() << "SynthrtEngine: could not open"
                                 << StringUtils::path_to_qstr(problem.path) << ":"
                                 << QString::fromStdString(problem.reason);
        }
        return complete;
    }

    /// Builds the unit and scans the voicebanks. The caller holds the lifecycle lock exclusively.
    bool start(const QStringList &voicebankPaths, const QStringList &packagePaths,
               const QString &ep, int deviceIndex, const fs::path &pluginRoot,
               const fs::path &runtimePath) {
        dependencyPaths = toPaths(packagePaths);
        pluginDirectory = pluginRoot;
        runtimeDirectory = runtimePath;
        backend = lite::synthrt::backendFromName(ep.toStdString());
        driverDeviceIndex = deviceIndex;
        const auto searched = searchPathsFor(toPaths(voicebankPaths));

        auto booted = lite::synthrt::Bootstrap::create(
            pluginRoot, searched, runtimePath, backend, deviceIndex);
        if (!booted) {
            qCritical().noquote() << "SynthrtEngine: the unit could not be built:"
                                  << QString::fromStdString(booted.error().toString());
            return false;
        }
        bootstrap = booted.take();
        language = std::make_unique<lite::synthrt::LanguageBridge>(bootstrap->unit());

        if (!bootstrap->hasDriver()) {
            qWarning() << "SynthrtEngine: no ONNX driver was found; inference is unavailable";
        }

        // Failed initialization leaves an empty catalog and releases temporary handles.
        std::vector<lite::synthrt::SingerEntry> entries;
        std::vector<srt::PackageHandle> opened;
        std::vector<lite::synthrt::PackageProblem> problems;
        if (scanInto(toPaths(voicebankPaths), entries, opened, problems)) {
            catalog = std::move(entries);
            packages = std::move(opened);
        }
        language->refresh();
        feedSingerPhonemes();
        return true;
    }

    using PackageSet = std::set<std::tuple<std::string, std::string, std::string>>;

    PackageSet packageSet() const {
        PackageSet result;
        for (const auto &package : packages) {
            result.emplace(package.id(), package.version().toString(), package.path().string());
        }
        return result;
    }

    /// Release language-session handles before reopening packages from disk.
    void releaseLanguageHandles() {
        if (!language) {
            return;
        }
        for (const auto &entry : catalog) {
            language->release({entry.packageId, entry.contributionId, entry.packageVersion});
        }
    }

    /// Refresh phoneme coverage and readiness under the exclusive lifecycle lock.
    void feedSingerPhonemes() {
        if (!language) {
            return;
        }
        lite::synthrt::feedSingerPhonemes(*language, catalog);
        // Capture readiness with the singer metadata, rather than querying a newer catalog later.
        for (auto &entry : catalog) {
            entry.convertibleLanguages.clear();
            for (const auto &handle : entry.capabilities.languages) {
                if (language->canConvert(
                        {entry.packageId, entry.contributionId, entry.packageVersion}, handle))
                    entry.convertibleLanguages.insert(handle);
            }
        }
    }

    void republish() {
        // Active callers keep their pipelines; new requests must use the new catalog.
        pipelines.clear();
        generation.fetch_add(1, std::memory_order_release);
    }

    void releasePackages() {
        republish();
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
#ifdef LITE_TEST_DATA_ROOT
    const auto testRoot = qEnvironmentVariable("DSEL_TEST_PLUGIN_ROOT");
    if (!testRoot.isEmpty() && QDir::isAbsolutePath(testRoot))
        return StringUtils::qstr_to_path(testRoot);
#endif
    // Return the parent of plugins/, as required by the shared deployment layout.
#if defined(Q_OS_MAC)
    return MacOSUtils::getMainBundlePath() / "Contents/PlugIns";
#elif defined(Q_OS_WIN)
    return stdc::system::application_directory();
#else
    return stdc::system::application_directory().parent_path() / "lib";
#endif
}

fs::path SynthrtEngine::defaultRuntimePath() {
    return defaultPluginRoot() / lite::synthrt::layout::ONNX_RUNTIME_DIR;
}

fs::path SynthrtEngine::defaultCudaRuntimePath() {
    return defaultRuntimePath() / lite::synthrt::layout::CUDA_RUNTIME_SUBDIR;
}

fs::path SynthrtEngine::defaultLanguagePackagePath() {
    return defaultPluginRoot() / lite::synthrt::layout::LANGUAGE_PACKAGES_DIR;
}

bool SynthrtEngine::initialize(const QStringList &voicebankPaths, const QStringList &packagePaths,
                               const QString &ep, int deviceIndex, const fs::path &pluginRoot,
                               const fs::path &runtimePath) {
    const auto announce = [this](bool ok) {
        _impl->initialized.store(ok, std::memory_order_release);
        completeInitializationAttempt();
        return ok;
    };

    std::unique_lock lock(_impl->lifecycle);
    if (_impl->aboutToQuit.load(std::memory_order_acquire)) {
        return announce(false);
    }
    try {
        return announce(
            _impl->start(voicebankPaths, packagePaths, ep, deviceIndex, pluginRoot, runtimePath));
    } catch (const std::exception &e) {
        // Complete the initialization attempt even when the SDK throws, releasing waiters.
        qCritical().noquote() << "SynthrtEngine: initialization failed:" << e.what();
        return announce(false);
    }
}


bool SynthrtEngine::initialized() const noexcept {
    return _impl->initialized.load(std::memory_order_acquire);
}

bool SynthrtEngine::inferenceReady() const {
    std::shared_lock lock(_impl->lifecycle);
    return _impl->bootstrap && _impl->bootstrap->hasDriver();
}

bool SynthrtEngine::initializationDone() const noexcept {
    return _impl->initializationDone.load(std::memory_order_acquire);
}

void SynthrtEngine::completeInitializationAttempt() noexcept {
    {
        std::lock_guard guard(_impl->doneMutex);
        _impl->initializationDone.store(true, std::memory_order_release);
    }
    _impl->done.notify_all();
}

bool SynthrtEngine::waitForInitialization(int timeoutMs) const {
    std::unique_lock guard(_impl->doneMutex);
    return _impl->done.wait_for(guard, std::chrono::milliseconds(timeoutMs),
                                [this] { return initializationDone(); });
}

bool SynthrtEngine::isAboutToQuit() const noexcept {
    return _impl->aboutToQuit.load(std::memory_order_acquire);
}

void SynthrtEngine::shutdown() noexcept {
    _impl->aboutToQuit.store(true, std::memory_order_release);
    std::unique_lock lock(_impl->lifecycle);
    _impl->initialized.store(false, std::memory_order_release);
    completeInitializationAttempt();

    // The application must still drain tasks before shutdown. A lease retains its runtime so its
    // executives can be destroyed safely, but it must not outlive the application's other state.
    for (const auto &[key, pipeline] : _impl->pipelines) {
        if (!pipeline.expired()) {
            qCritical().noquote().nospace()
                << "SynthrtEngine: shutting down while the pipeline of "
                << QString::fromStdString(std::get<0>(key)) << "@"
                << QString::fromStdString(std::get<1>(key)) << "/"
                << QString::fromStdString(std::get<2>(key)) << " is still held";
        }
    }
    _impl->releasePackages();
    // The language session owns its own executives and must be destroyed before the unit.
    _impl->language.reset();
    _impl->catalog.clear();
    if (_impl->bootstrap) {
        for (const auto &package : _impl->bootstrap->unit().loadedPackages()) {
            qCritical().noquote().nospace()
                << "SynthrtEngine: shutting down while the package "
                << QString::fromStdString(package.id()) << "@"
                << QString::fromStdString(package.version().toString())
                << " is still loaded by another holder";
        }
    }
    _impl->bootstrap.reset();
}

srt::Expected<std::vector<lite::synthrt::SingerEntry>>
    SynthrtEngine::refreshVoicebanks(const std::vector<fs::path> &searchPaths,
                                     std::vector<lite::synthrt::PackageProblem> *problems,
                                     RescanMode mode,
                                     std::vector<lite::synthrt::PackageEntry> *packages) {
    // Release idle app caches before taking the SDK lock: their DirectML teardown guard has the
    // same lock order as an inference task (provider serialization, then SDK lifecycle).
    if (mode == RescanMode::Reload)
        emit voicebanksAboutToReload();
    std::unique_lock lock(_impl->lifecycle);
    if (!_impl->bootstrap) {
        return srt::Error(srt::Error::InvalidArgument, "the engine is not initialized");
    }
    std::shared_ptr<lite::synthrt::Bootstrap> reloaded;
    if (mode == RescanMode::Reload) {
        auto created = lite::synthrt::Bootstrap::create(
            _impl->pluginDirectory, _impl->searchPathsFor(searchPaths), _impl->runtimeDirectory,
            _impl->backend, _impl->driverDeviceIndex);
        if (!created)
            return created.takeError();
        reloaded = created.take();
    } else {
        _impl->bootstrap->unit().setPackagePaths(_impl->searchPathsFor(searchPaths));
    }

    // Reload uses a fresh unit; active leases retain the previous unit.
    const auto before = _impl->packageSet();

    // Failed Reload leaves an empty catalog. Failed ReuseLoaded restores the previous handles
    // and their matching catalog.
    std::vector<srt::PackageHandle> previous;
    std::vector<lite::synthrt::SingerEntry> previousCatalog;
    if (mode == RescanMode::Reload) {
        _impl->republish();
        _impl->language.reset();
        _impl->packages.clear();
        _impl->catalog.clear();
        _impl->bootstrap = std::move(reloaded);
        _impl->language =
            std::make_unique<lite::synthrt::LanguageBridge>(_impl->bootstrap->unit());
    } else {
        previous = std::move(_impl->packages);
        previousCatalog = std::move(_impl->catalog);
    }

    // Publish handles and catalog together only after a successful scan.
    std::vector<lite::synthrt::SingerEntry> entries;
    std::vector<srt::PackageHandle> opened;
    std::vector<lite::synthrt::PackageProblem> failures;
    if (_impl->scanInto(searchPaths, entries, opened, failures)) {
        _impl->catalog = std::move(entries);
        _impl->packages = std::move(opened);
        previous.clear();
    } else if (mode == RescanMode::ReuseLoaded) {
        _impl->catalog = std::move(previousCatalog);
        _impl->packages = std::move(previous);
    }

    // Reload already invalidated every old pipeline, even if its package identity is unchanged.
    if (mode == RescanMode::ReuseLoaded && _impl->packageSet() != before) {
        _impl->republish();
    }
    if (problems != nullptr) {
        *problems = std::move(failures);
    }
    if (_impl->language) {
        _impl->language->refresh();
        // Reapply phoneme inventories after refreshing the language catalog.
        _impl->feedSingerPhonemes();
    }
    if (packages) {
        packages->clear();
        for (const auto &package : _impl->packages) {
            packages->push_back({package.id(), package.version(), package.path(), package.vendor(),
                                 package.description(), package.copyright(), package.url()});
        }
    }
    return _impl->catalog;
}

srt::Expected<void> SynthrtEngine::validatePackage(const fs::path &path) const {
    srt::SynthUnit isolated;
    {
        std::shared_lock lock(_impl->lifecycle);
        if (!_impl->bootstrap)
            return srt::Error(srt::Error::InvalidArgument, "the engine is not initialized");
        const auto &unit = _impl->bootstrap->unit();
        auto packagePaths = unit.packagePaths();
        std::error_code error;
        const auto parent = fs::absolute(path, error).parent_path();
        if (error)
            return srt::Error(srt::Error::InvalidArgument, error.message());
        if (std::find(packagePaths.begin(), packagePaths.end(), parent) == packagePaths.end())
            packagePaths.push_back(parent);
        isolated.setPackagePaths(packagePaths);
        for (const auto &category : lite::synthrt::pluginCategories({}))
            isolated.setPluginPaths(category.category, unit.pluginPaths(category.category));
    }
    auto opened = isolated.openPackage(path, srt::SynthUnit::Load);
    if (!opened)
        return opened.takeError();
    return {};
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

srt::Expected<std::shared_ptr<lite::synthrt::SingerPipeline>>
    SynthrtEngine::pipelineFor(const SingerIdentifier &identifier) {
    std::unique_lock lock(_impl->lifecycle);
    if (!_impl->bootstrap) {
        return srt::Error(srt::Error::InvalidArgument, "the engine is not initialized");
    }
    // Resolve the same package version for metadata and pipeline lookup.
    const auto key = Impl::pipelineKey(_impl->resolved(identifier));
    if (const auto it = _impl->pipelines.find(key); it != _impl->pipelines.end()) {
        if (auto held = it->second.lock()) {
            return held;
        }
        _impl->pipelines.erase(it);
    }
    const auto &[packageId, packageVersion, contributionId] = key;

    // Borrow the declaration through a retained package handle.
    srt::ContribSpec *spec = nullptr;
    const srt::PackageHandle *owner = nullptr;
    for (auto &package : _impl->packages) {
        if (package.id() == packageId && package.version().toString() == packageVersion) {
            spec = package.contribution(srt::SingerCategory::NAME, contributionId);
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
    struct OwnedPipeline {
        std::shared_ptr<lite::synthrt::Bootstrap> unit;
        std::unique_ptr<lite::synthrt::SingerPipeline> pipeline;
    };
    auto pipelineOwner = std::make_shared<OwnedPipeline>();
    pipelineOwner->unit = _impl->bootstrap;
    pipelineOwner->pipeline = built.take();
    std::shared_ptr<lite::synthrt::SingerPipeline> pipeline(pipelineOwner,
                                                         pipelineOwner->pipeline.get());
    _impl->pipelines[key] = pipeline;
    return pipeline;
}

std::vector<lite::synthrt::AnalyzerEntry>
    SynthrtEngine::analyzers(const QString &interfaceName) const {
    std::shared_lock lock(_impl->lifecycle);
    auto found = lite::synthrt::analyzersOf(_impl->packages);
    // Deduplicate versionless analyzer references using the version createAnalyzer() selects.
    std::sort(found.begin(), found.end(), [](const auto &a, const auto &b) {
        return std::tie(a.packageId, a.contributionId, b.packageVersion) <
               std::tie(b.packageId, b.contributionId, a.packageVersion);
    });
    const auto wanted = interfaceName.toStdString();
    std::vector<lite::synthrt::AnalyzerEntry> result;
    std::optional<std::pair<std::string, std::string>> previous;
    for (auto &entry : found) {
        const auto key = std::pair{entry.packageId, entry.contributionId};
        if (previous == key) {
            continue;
        }
        previous = key;
        if (!wanted.empty() && entry.interfaceName != wanted) {
            continue;
        }
        result.push_back(std::move(entry));
    }
    return result;
}

srt::ContribSpec *SynthrtEngine::findAnalyzer(const QString &reference,
                                              const srt::PackageHandle **package) const {
    const auto parsed = lite::synthrt::AnalyzerReference::parse(reference.toStdString());
    if (!parsed) {
        return nullptr;
    }
    srt::ContribSpec *best = nullptr;
    const srt::PackageHandle *bestPackage = nullptr;
    for (const auto &candidate : _impl->packages) {
        if (candidate.id() != parsed->packageId) {
            continue;
        }
        if (bestPackage != nullptr && !(bestPackage->version() < candidate.version())) {
            continue;
        }
        auto *spec = candidate.contribution(srt::InferenceCategory::NAME, parsed->contributionId);
        if (spec != nullptr && lite::synthrt::isAnalysisContract(spec->interface())) {
            best = spec;
            bestPackage = &candidate;
        }
    }
    if (best != nullptr && package != nullptr) {
        *package = bestPackage;
    }
    return best;
}

srt::Expected<SynthrtEngine::AnalyzerLease>
    SynthrtEngine::createAnalyzer(const QString &reference) {
    std::shared_lock lock(_impl->lifecycle);
    const srt::PackageHandle *package = nullptr;
    auto *spec = findAnalyzer(reference, &package);
    if (spec == nullptr) {
        return srt::Error(srt::Error::FileNotFound,
                          "no installed package holds the analyzer "
                              + reference.toStdString());
    }
    const auto *contract = lite::synthrt::findAnalysisContract(spec->interface());
    auto *inference = spec->as<srt::InferenceSpec>();
    if (contract == nullptr || inference == nullptr) {
        return srt::Error(srt::Error::FeatureNotSupported,
                          "the editor does not run analyzers of the contract " +
                              spec->interface());
    }
    auto created = contract->create(*inference);
    if (!created) {
        return created.takeError();
    }
    AnalyzerLease lease;
    lease.unitOwner = _impl->bootstrap;
    lease.package = *package;
    lease.spec = spec;
    lease.executive = created.take();
    return lease;
}

bool SynthrtEngine::canConvert(const SingerIdentifier &identifier, const QString &language) const {
    std::shared_lock lock(_impl->lifecycle);
    if (!_impl->language) {
        return false;
    }
    return _impl->language->canConvert(singerOf(_impl->resolved(identifier)), language.toStdString());
}

QString SynthrtEngine::languageUnavailableReason(const SingerIdentifier &identifier,
                                                 const QString &language) const {
    // Leave fallback wording to the UI when no language layer is available.
    std::shared_lock lock(_impl->lifecycle);
    if (!_impl->language) {
        return QString();
    }
    const auto singer = singerOf(_impl->resolved(identifier));
    const auto reason = _impl->language->unavailableReason(singer, language.toStdString());
    return QString::fromStdString(reason);
}

std::optional<lite::synthrt::LanguageBridge::Depth>
    SynthrtEngine::maxDepth(const SingerIdentifier &identifier, const QString &language) const {
    std::shared_lock lock(_impl->lifecycle);
    if (!_impl->language) {
        return std::nullopt;
    }
    return _impl->language->maxDepth(singerOf(_impl->resolved(identifier)), language.toStdString());
}

srt::Expected<std::vector<lite::synthrt::LanguageBridge::Result>>
    SynthrtEngine::convert(const SingerIdentifier &identifier, const QString &language,
                           const std::vector<lite::synthrt::LanguageBridge::Word> &words,
                           lite::synthrt::LanguageBridge::Depth depth) const {
    srt::Expected<std::vector<lite::synthrt::LanguageBridge::Result>> converted;
    {
        std::shared_lock lock(_impl->lifecycle);
        if (!_impl->language) {
            return srt::Error(srt::Error::InvalidArgument, "the engine is not initialized");
        }
        converted = _impl->language->convert(singerOf(_impl->resolved(identifier)),
                                             language.toStdString(), words, depth);
    }
    if (!converted) {
        // Emit outside the lifecycle lock: receivers may query language availability immediately.
        emit SynthrtEngine::instance().languageRouteFailed(language);
    }
    return converted;
}

std::vector<std::string> SynthrtEngine::reservedPhonemesOf(const SingerIdentifier &identifier) const {
    std::shared_lock lock(_impl->lifecycle);
    const auto *entry = _impl->find(identifier);
    return lite::synthrt::withForcedReservedPhonemes(
        entry ? entry->capabilities.reservedPhonemes : std::vector<std::string>());
}

srt::SynthUnit *SynthrtEngine::unitIfReady() noexcept {
    std::shared_lock lock(_impl->lifecycle);
    return _impl->bootstrap ? &_impl->bootstrap->unit() : nullptr;
}
