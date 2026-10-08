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

    /// Scans \a paths into the caller's containers and logs every package that failed to open.
    ///
    /// The results are written to \a entries and \a opened rather than to the members, so that a
    /// scan that fails halfway leaves no half-batch of handles behind: a handle keeps its package
    /// loaded, and a catalog that describes a different set of packages than the handles held is a
    /// state no caller can use. The caller commits the containers only if this function returns
    /// true; see refreshVoicebanks().
    ///
    /// scan() reports filesystem failures as problems. An exception from a lower layer is also
    /// caught here, because the scan runs on a worker thread where an uncaught exception
    /// terminates the process, and the contents of a voicebank directory must not terminate the
    /// process.
    ///
    /// The search paths are scanned one by one, so that an exception can name the path it came
    /// from; the problem recorded for it must carry the path, because the UI shows it as the
    /// "voicebank that cannot be opened" line. Calling scan() once per path is equivalent to a
    /// single call over all of them: scan() appends to \a opened and problems path by path, in
    /// order, so the entries, the handles, and the content and order of the problems are unchanged.
    ///
    /// \return true if every path was scanned; false if a path raised an exception.
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

        // The scan at initialization has no previous state to recover: both members are empty to
        // begin with, so a failure means the same as a failed Reload and lands in the same empty
        // state — the local containers release the half batch of handles when they are destroyed,
        // and the members stay empty.
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

    /// Supplies the phoneme table that the catalog derived for every singer to the language layer,
    /// so that it reports the coverage of a language instead of an unknown coverage.
    ///
    /// The table comes from the catalog rather than from a read of the voicebank here, because the
    /// derivation is a property of the singer and belongs beside the other capabilities. The call
    /// follows every refresh() of the language layer, because a table survives a refresh only for
    /// a singer that the new catalog still holds. The caller holds the lifecycle lock exclusively,
    /// like every caller of refresh(), and the language layer synchronizes its own state.
    void feedSingerPhonemes() {
        if (!language) {
            return;
        }
        lite::synthrt::feedSingerPhonemes(*language, catalog);
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

    // The failure semantics must differ by mode, because the two modes differ in whether the
    // previous state can be recovered:
    //   * Reload has already released the handles of the language session and dropped the engine's
    //     handles before the scan, and most packages that were read are no longer in the unit, so
    //     the old handles cannot physically be recovered, and a failure can only land in the
    //     consistent empty state, in which both the catalog and the handles are empty. Dropping
    //     only the handles while keeping the old catalog would leave a self-contradictory state,
    //     one in which the catalog describes the old packages and packages holds half of the new
    //     ones, and downstream code (PackageManager, the language layer) would resolve singers
    //     that do not exist from it.
    //   * ReuseLoaded moves the old handles and the matching old catalog into previous together and
    //     holds them throughout the scan, so no package is released, and a failure can restore both
    //     as they were: the user still sees the voicebank list from before the scan.
    std::vector<srt::PackageHandle> previous;
    std::vector<lite::synthrt::SingerEntry> previousCatalog;
    if (mode == RescanMode::Reload) {
        _impl->releaseLanguageHandles();
        _impl->packages.clear();
        _impl->catalog.clear();
    } else {
        previous = std::move(_impl->packages);
        previousCatalog = std::move(_impl->catalog);
    }

    // The only commit point: the scan writes to local containers only, and they are committed in
    // one step only if everything succeeded. On failure the half batch of handles stays in the
    // local containers and is released with their destruction, never attached to the members.
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
    // After a failed Reload both members were cleared above (the consistent empty state); problems
    // always hold the reasons found by this scan.

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
        // A table survives a refresh only for a singer that the new catalog still holds, so the
        // tables are supplied again for the catalog that was just committed. The caller holds the
        // lifecycle lock exclusively, exactly as it does for the refresh above, and the helper
        // takes no lock of its own.
        _impl->feedSingerPhonemes();
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

bool SynthrtEngine::canConvert(const SingerIdentifier &identifier, const QString &language) const {
    std::shared_lock lock(_impl->lifecycle);
    if (!_impl->language) {
        return false;
    }
    return _impl->language->canConvert(singerOf(_impl->resolved(identifier)), language.toStdString());
}

QString SynthrtEngine::languageUnavailableReason(const SingerIdentifier &identifier,
                                                 const QString &language) const {
    // The lock and the precondition match canConvert() exactly: canConvert() returns false when the
    // engine is not initialized, and this call treats that the same way, as "no route is
    // available". The reason text, however, can only come from wolf, and this layer invents none,
    // so an empty string is returned (the UI has a fallback text).
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
    // The lock and the precondition match canConvert(): without a language layer the query means
    // the same as the false of canConvert(), and an empty value is returned.
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
        // The notification is emitted outside the lock: the receiving slot queries language
        // availability again, and that query takes the same lifecycle lock, which a direct
        // connection in the same thread would re-enter. Emitting through the singleton rather than
        // through this object is not a workaround: the engine is only ever constructed by
        // SingletonRegistry (see the friend declaration in the header), so instance() is this
        // object whenever convert() runs, and convert() stays const.
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
    // The returned pointer remains valid only until shutdown(), so the lock protects only the
    // read: it prevents the read below from racing the reset in shutdown(), and the documented
    // lifetime covers later use. Returning nullptr instead of terminating the process allows a
    // caller that may run before initialize() or after shutdown() to handle the absence.
    std::shared_lock lock(_impl->lifecycle);
    return _impl->bootstrap ? &_impl->bootstrap->unit() : nullptr;
}
