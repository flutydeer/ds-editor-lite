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

    /// Returns the language layer's address for a singer, including the package version.
    ///
    /// The version is included because the specification allows two versions of one package to be
    /// loaded at the same time, and a module reference cannot distinguish them. A lookup without
    /// the version succeeds only while exactly one version is installed, and silently returns no
    /// result if two versions are installed.
    lite::synthrt::LanguageBridge::Singer singerOf(const SingerIdentifier &identifier) {
        const auto [packageId, contributionId] = identifier.contribution();
        return {packageId, contributionId, VersionUtils::qt_to_stdc(identifier.packageVersion)};
    }

}

class SynthrtEngine::Impl {
public:
    /// Guards the engine state against a concurrent shutdown.
    ///
    /// Held shared while the unit is in use, and exclusively while the unit is replaced or
    /// destroyed. It corresponds to the runtime lifecycle lock of the refactor line and exists for
    /// the same reason: a package handle, a pipeline and a conversion all borrow from the unit,
    /// and none of them tolerates the destruction of the unit during use.
    mutable std::shared_mutex lifecycle;

    std::unique_ptr<lite::synthrt::Bootstrap> bootstrap;
    std::unique_ptr<lite::synthrt::LanguageBridge> language;

    /// The packages that the last scan loaded. The handles keep them loaded until a refresh.
    std::vector<srt::PackageHandle> packages;
    std::vector<lite::synthrt::SingerEntry> catalog;

    /// The pipelines currently held by callers, keyed by the editor's singer identity: package,
    /// version and contribution. The key includes the version for the same reason as singerOf():
    /// two versions of one package may be loaded at the same time, and a pipeline built from one
    /// version must not be returned for the other. The pointers are weak because the holders own
    /// the pipelines; the table only allows two holders to share a pipeline.
    using PipelineKey = std::tuple<std::string, std::string, std::string>;
    std::map<PipelineKey, std::weak_ptr<lite::synthrt::SingerPipeline>> pipelines;

    static PipelineKey pipelineKey(const SingerIdentifier &identifier) {
        const auto [packageId, contributionId] = identifier.contribution();
        return {packageId, VersionUtils::qt_to_stdc(identifier.packageVersion).toString(),
                contributionId};
    }

    /// Incremented on every republication of the catalog, so that a caller holding a pipeline
    /// can detect that the pipeline describes an earlier catalog.
    std::atomic_uint64_t generation = 0;

    std::atomic_bool initialized = false;
    std::atomic_bool initializationDone = false;
    std::atomic_bool aboutToQuit = false;

    mutable std::mutex doneMutex;
    mutable std::condition_variable done;

    /// Returns the catalog entry of a singer, or null if no entry matches. The caller holds the
    /// lifecycle lock.
    ///
    /// The version is part of the identity if the identifier specifies a version, for the same
    /// reason that pipelineKey() and singerOf() include it: two versions of one package may be
    /// loaded at the same time, and the entry of the other version would report the languages,
    /// speakers and directory of a different voicebank. An identifier without a version resolves
    /// to the highest loaded version, so that the result does not depend on the order of the
    /// directory scan. Every lookup of the engine uses this rule, including pipelineFor() and the
    /// language calls.
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

    /// Returns \a identifier with the version of the entry that find() selects, or unchanged if
    /// no entry matches. The caller holds the lifecycle lock.
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

    /// Scans \a paths into the catalog and logs every package that failed to open.
    ///
    /// scan() reports filesystem failures as problems. An exception from a lower layer is also
    /// caught here, because the scan runs on a worker thread where an uncaught exception
    /// terminates the process, and the contents of a voicebank directory must not terminate the
    /// process.
    void scanInto(const std::vector<fs::path> &paths,
                  std::vector<lite::synthrt::PackageProblem> &problems) {
        try {
            catalog = lite::synthrt::scan(bootstrap->unit(), paths, packages, problems);
        } catch (const std::exception &e) {
            problems.push_back({fs::path(), std::string("the scan failed: ") + e.what()});
        }
        for (const auto &problem : problems) {
            qWarning().noquote() << "SynthrtEngine: could not open"
                                 << StringUtils::path_to_qstr(problem.path) << ":"
                                 << QString::fromStdString(problem.reason);
        }
    }

    /// Builds the unit and scans the voicebanks. The caller holds the lifecycle lock exclusively.
    bool start(const QStringList &voicebankPaths, const QStringList &packagePaths,
               const QString &ep, int deviceIndex, const fs::path &pluginRoot,
               const fs::path &runtimePath) {
        auto searched = toPaths(packagePaths);
        for (auto &path : toPaths(voicebankPaths)) {
            // A voicebank directory is also a dependency search path, because a language package
            // may be installed beside the voicebanks that depend on it.
            searched.push_back(std::move(path));
        }

        auto booted = lite::synthrt::Bootstrap::create(
            pluginRoot, searched, runtimePath, lite::synthrt::backendFromName(ep.toStdString()),
            deviceIndex);
        if (!booted) {
            qCritical().noquote() << "SynthrtEngine: the unit could not be built:"
                                  << QString::fromStdString(booted.error().toString());
            return false;
        }
        bootstrap = booted.take();
        language = std::make_unique<lite::synthrt::LanguageBridge>(bootstrap->unit());

        if (!bootstrap->hasDriver()) {
            // Initialization continues in a degraded state rather than failing: voicebanks are
            // still listed and projects still open, and a warning serves the user better than an
            // editor that refuses to start.
            qWarning() << "SynthrtEngine: no ONNX driver was found; inference is unavailable";
        }

        std::vector<lite::synthrt::PackageProblem> problems;
        scanInto(toPaths(voicebankPaths), problems);
        language->refresh();
        return true;
    }

    /// The identity of every package the engine holds, as package id, version and directory.
    using PackageSet = std::set<std::tuple<std::string, std::string, std::string>>;

    PackageSet packageSet() const {
        PackageSet result;
        for (const auto &package : packages) {
            result.emplace(package.id(), package.version().toString(), package.path().string());
        }
        return result;
    }

    /// Releases the handles of the language session for every cataloged singer, so that a
    /// subsequent rescan reads their packages from disk instead of reusing the loaded packages.
    void releaseLanguageHandles() {
        if (!language) {
            return;
        }
        for (const auto &entry : catalog) {
            language->release({entry.packageId, entry.contributionId, entry.packageVersion});
        }
    }

    /// Marks every pipeline taken so far as describing an earlier catalog.
    void republish() {
        // A pipeline that is still held keeps its own package handle and remains valid. The
        // table of shared pipelines is cleared so that the next request builds a pipeline from
        // the new scan instead of sharing a pipeline from an earlier scan.
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
    // The returned directory contains the plugin tree and is not the tree itself. Each of the
    // three packages installs under plugins/<library>/<category> below this directory, and
    // Bootstrap appends the remaining components. This directory must therefore be their common
    // parent; otherwise every derived path is off by one level.
#if defined(Q_OS_MAC)
    return MacOSUtils::getMainBundlePath() / "Contents/PlugIns";
#elif defined(Q_OS_WIN)
    return stdc::system::application_directory();
#else
    return stdc::system::application_directory().parent_path() / "lib";
#endif
}

fs::path SynthrtEngine::defaultRuntimePath() {
    // The build deploys ONNX Runtime beside the driver plugin. The path is specified rather than
    // searched for, because the choice of the ONNX Runtime copy is a deployment decision, and a
    // driver that searches may load a different copy from the shipped copy.
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
    try {
        return announce(
            _impl->start(voicebankPaths, packagePaths, ep, deviceIndex, pluginRoot, runtimePath));
    } catch (const std::exception &e) {
        // Threads blocked in waitForInitialization() must receive the result, so a failure
        // reported as an exception is announced like any other failure instead of leaving the
        // waiting threads to time out.
        qCritical().noquote() << "SynthrtEngine: initialization failed:" << e.what();
        return announce(false);
    }
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

    // The owner must release every pipeline and analyzer lease before shutdown() runs, because
    // each keeps a package of the unit loaded, and synthrt terminates the process if a package is
    // released while its executives are alive. This function cannot enforce the order, so it logs
    // each violation before the unit is destroyed.
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
                                     RescanMode mode) {
    std::unique_lock lock(_impl->lifecycle);
    if (!_impl->bootstrap) {
        return srt::Error(srt::Error::InvalidArgument, "the engine is not initialized");
    }

    // synthrt returns a package that is still loaded instead of reading it again, so the handles
    // held during the scan determine whether a package is read from disk. A reload releases the
    // handles of the language session and the engine first, so that a voicebank changed on disk is
    // read again unless a running synthesis still holds it. Reuse keeps the handles until the scan
    // has taken the packages over, which makes a scan of an unchanged set inexpensive.
    const auto before = _impl->packageSet();
    std::vector<srt::PackageHandle> previous;
    if (mode == RescanMode::Reload) {
        _impl->releaseLanguageHandles();
        _impl->packages.clear();
    } else {
        previous = std::move(_impl->packages);
        _impl->packages.clear();
    }

    std::vector<lite::synthrt::PackageProblem> failures;
    _impl->scanInto(searchPaths, failures);
    previous.clear();

    // Only a change in the set of packages republishes the catalog. A rescan that finds the same
    // packages keeps the held pipelines current instead of marking every singer session as stale.
    if (_impl->packageSet() != before) {
        _impl->republish();
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
    // An identifier without a version is resolved as find() resolves it, so that the pipeline
    // and the catalog entry that describes it belong to the same package version.
    const auto key = Impl::pipelineKey(_impl->resolved(identifier));
    if (const auto it = _impl->pipelines.find(key); it != _impl->pipelines.end()) {
        if (auto held = it->second.lock()) {
            return held;
        }
        _impl->pipelines.erase(it);
    }
    const auto &[packageId, packageVersion, contributionId] = key;

    // The declaration is obtained through the package handle rather than stored in the catalog.
    // A ContribSpec belongs to its package, and the pipeline stores the handle so that the
    // declaration remains valid across refreshes for the lifetime of the pipeline.
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
    std::shared_ptr<lite::synthrt::SingerPipeline> pipeline(built.take().release());
    _impl->pipelines[key] = pipeline;
    return pipeline;
}

std::vector<lite::synthrt::AnalyzerEntry>
    SynthrtEngine::analyzers(const QString &interfaceName) const {
    std::shared_lock lock(_impl->lifecycle);
    auto found = lite::synthrt::analyzersOf(_impl->packages);
    // A reference specifies no version, so only the version that createAnalyzer() runs is listed.
    // Listing the other versions would produce several entries for the same stored setting.
    std::sort(found.begin(), found.end(), [](const auto &a, const auto &b) {
        return std::tie(a.packageId, a.contributionId, b.packageVersion) <
               std::tie(b.packageId, b.contributionId, a.packageVersion);
    });
    const auto wanted = interfaceName.toStdString();
    std::vector<lite::synthrt::AnalyzerEntry> result;
    for (auto &entry : found) {
        if (!result.empty() && result.back().packageId == entry.packageId &&
            result.back().contributionId == entry.contributionId) {
            continue;
        }
        if (!wanted.empty() && entry.interfaceName != wanted) {
            continue;
        }
        result.push_back(std::move(entry));
    }
    return result;
}

srt::ContribSpec *SynthrtEngine::findAnalyzer(const QString &reference,
                                              const srt::PackageHandle **package) const {
    // The caller holds the lifecycle lock.
    const auto parsed = lite::synthrt::AnalyzerReference::parse(reference.toStdString());
    if (!parsed) {
        return nullptr;
    }
    // A reference specifies no version. Of the loaded versions that contain the analyzer, the
    // highest version is used, as analyzers() lists it, so that the choice does not depend on the
    // scan order.
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
        // An inference module with any other contract is not an analyzer, regardless of the
        // reference.
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
    // Each contract creates its own executive type, so the contract table selects the factory.
    // findAnalyzer() accepts only contracts in that table. The check below prevents a contract
    // without a factory from being created as another contract if the two ever diverge.
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
    for (const auto &language : _impl->language->languagesOf(singerOf(_impl->resolved(identifier)))) {
        result << QString::fromStdString(language);
    }
    return result;
}

bool SynthrtEngine::canConvert(const SingerIdentifier &identifier, const QString &language) const {
    std::shared_lock lock(_impl->lifecycle);
    if (!_impl->language) {
        return false;
    }
    return _impl->language->canConvert(singerOf(_impl->resolved(identifier)), language.toStdString());
}

srt::Expected<std::vector<lite::synthrt::LanguageBridge::Result>>
    SynthrtEngine::convert(const SingerIdentifier &identifier, const QString &language,
                           const std::vector<lite::synthrt::LanguageBridge::Word> &words,
                           lite::synthrt::LanguageBridge::Depth depth) const {
    std::shared_lock lock(_impl->lifecycle);
    if (!_impl->language) {
        return srt::Error(srt::Error::InvalidArgument, "the engine is not initialized");
    }
    return _impl->language->convert(singerOf(_impl->resolved(identifier)), language.toStdString(), words,
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
    _impl->language->setSingerPhonemes(singerOf(_impl->resolved(identifier)), std::move(phonemes));
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

std::vector<std::string> SynthrtEngine::reservedPhonemesOf(const SingerIdentifier &identifier) const {
    std::shared_lock lock(_impl->lifecycle);
    const auto *entry = _impl->find(identifier);
    return lite::synthrt::withForcedReservedPhonemes(
        entry ? entry->capabilities.reservedPhonemes : std::vector<std::string>());
}

srt::SynthUnit *SynthrtEngine::unitIfReady() noexcept {
    // The returned pointer remains valid only until shutdown(), so the lock protects only the
    // read: it prevents the read below from racing the reset in shutdown(), and the documented
    // lifetime covers later use. Returning nullptr instead of terminating the process allows a
    // caller that may run before initialize() or after shutdown() to handle the absence.
    std::shared_lock lock(_impl->lifecycle);
    return _impl->bootstrap ? &_impl->bootstrap->unit() : nullptr;
}

srt::SynthUnit &SynthrtEngine::unit() {
    // The caller must guarantee the precondition: a successful initialize() and no shutdown()
    // since. A reference cannot represent an absent unit, so a violation terminates the process
    // in every build configuration with a message that identifies the cause, instead of the
    // undefined behavior of dereferencing nullptr. instance() terminates the process in the same
    // way for an unregistered engine. A caller that cannot guarantee the precondition calls
    // unitIfReady() instead and tests the result.
    auto *ready = unitIfReady();
    if (ready == nullptr) {
        qFatal("SynthrtEngine::unit() requires an initialized engine; use unitIfReady() if the "
               "engine may be uninitialized");
    }
    return *ready;
}
