//
// SynthrtEngine implementation — v2 component facade.
//
// Reference: docs/refactoring-v2/03-lite-integration.md
//

#include "SynthrtEngine.h"
#include "SingerModelSession.h"

#include "AppContext.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "Utils/StringUtils.h"
#include "Utils/VersionUtils.h"

#include <stdcorelib/path.h>
#include <stdcorelib/system.h>

#include <synthrt/G2P/Base/LangCommon.h>
#include <synthrt/G2P/Core/Manager.h>
#include <synthrt/G2P/Task/SessionTask.h>
#include <synthrt/G2P/Task/SessionFactory.h>
#include <synthrt/G2P/Task/TaskPlugin.h>
#include <synthrt/Driver/InferenceDriver.h>
#include <synthrt/Driver/InferenceSession.h>
#include <synthrt/Driver/onnx/OnnxDriverApi.h>
#include <synthrt/Driver/OnnxSetup.h>
#include <synthrt/Core/Core/Runtime.h>
#include <synthrt/Core/Module/Module.h>
#include <synthrt/Core/Plugin/PluginFactory.h>
#include <synthrt/Extract/PitchExtractorPlugin.h>
#include <synthrt/Extract/MidiExtractorPlugin.h>

#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QReadWriteLock>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <cwctype>
#include <thread>
#include <vector>

#if defined(Q_OS_MAC)
#  include "Utils/MacOSUtils.h"
#endif

namespace fs = std::filesystem;

static srt::core::Expected<void> checkPath(const std::filesystem::path &path) {
    if (!std::filesystem::exists(path)) {
        return srt::core::Error(srt::core::ErrorCode::FileNotFound,
                                "Path does not exist: " + stdc::path::to_utf8(path));
    }
    if (!std::filesystem::is_directory(path)) {
        return srt::core::Error(srt::core::ErrorCode::InvalidArgument,
                                "Path is not a directory: " + stdc::path::to_utf8(path));
    }
    return srt::core::Expected<void>();
}

// ============================================================================
// G2P ONNX driver adapters — reuse the inference ONNX driver with CPU forced.
//
// Design: G2P and inference share the same ONNX plugin (srt-onnxdriver). The
// G2P side wraps the inference InferenceDriver/InferenceSession, translating
// G2P SessionFactory/SessionTask calls and forcing useCpu=true on every
// session open() so G2P never competes with GPU inference.
// ============================================================================

namespace {

    fs::path stablePackagePath(const fs::path &path) {
        std::error_code error;
        auto stable = fs::weakly_canonical(path, error);
        if (error) {
            stable = path.lexically_normal();
        }
#if defined(Q_OS_WIN)
        auto native = stable.native();
        std::transform(native.begin(), native.end(), native.begin(), [](const auto ch) {
            return static_cast<decltype(ch)>(std::towlower(ch));
        });
        return fs::path(native);
#else
        return stable;
#endif
    }

    /// G2P ONNX SessionTask adapter — wraps an inference InferenceSession.
    class G2pOnnxSessionTask : public srt::g2p::SessionTask {
    public:
        explicit G2pOnnxSessionTask(srt::core::NO<srt::driver::InferenceSession> session)
            : m_inner(std::move(session)) {
        }

        int apiLevel() const override {
            return 0;
        }

        srt::core::Expected<void> initialize() override {
            return {};
        }

        srt::core::Expected<void>
            open(const std::filesystem::path &path,
                 const srt::core::NO<srt::core::TaskInitArgs> &args) override {
            auto inferenceArgs = srt::core::NO<srt::driver::onnx::SessionOpenArgs>::create();
            inferenceArgs->useCpu = true; // G2P always runs on CPU
            return m_inner->open(path, inferenceArgs);
        }

        srt::core::Expected<void> close() override {
            return m_inner->close();
        }

        bool isOpen() const override {
            return m_inner->isOpen();
        }

        int64_t id() const override {
            return m_inner->id();
        }

        srt::core::Expected<srt::core::NO<srt::core::TaskResult>>
            start(const srt::core::NO<srt::core::TaskStartInput> &input) override {
            // Translate G2P SessionStartInput → inference SessionStartInput
            auto inferenceInput = srt::core::NO<srt::driver::onnx::SessionStartInput>::create();
            auto g2pInput = input.as<srt::g2p::SessionStartInput>();
            if (g2pInput) {
                inferenceInput->inputs = g2pInput->inputs;
                inferenceInput->outputs = g2pInput->outputs;
            }
            auto result = m_inner->start(inferenceInput);
            if (!result)
                return result.error();
            // Translate inference SessionResult → G2P SessionResult
            auto g2pResult = srt::core::NO<srt::g2p::SessionResult>::create();
            auto inferenceResult = result.take().as<srt::driver::onnx::SessionResult>();
            if (inferenceResult) {
                g2pResult->outputs = std::move(inferenceResult->outputs);
            }
            return g2pResult;
        }

    private:
        srt::core::NO<srt::driver::InferenceSession> m_inner;
    };

    /// G2P ONNX SessionFactory adapter — wraps an inference InferenceDriver.
    /// Holds a shared_ptr to the driver so the adapter remains valid even if
    /// the Runtime's ObjectPool is destroyed first during shutdown.
    class G2pOnnxSessionFactory : public srt::g2p::SessionFactory {
    public:
        explicit G2pOnnxSessionFactory(srt::core::NO<srt::driver::InferenceDriver> driver)
            : m_driver(std::move(driver)) {
        }

        std::string arch() const override {
            return m_driver->arch();
        }

        std::string backend() const override {
            return m_driver->backend();
        }

        srt::core::Expected<void>
            initialize(const srt::core::NO<srt::core::TaskInitArgs> &args) override {
            return {}; // Inference driver already initialized
        }

        srt::core::NO<srt::g2p::SessionTask> createSession() override {
            auto session = m_driver->createSession();
            if (!session)
                return nullptr;
            return srt::core::NO<G2pOnnxSessionTask>::create(std::move(session));
        }

    private:
        srt::core::NO<srt::driver::InferenceDriver> m_driver;
    };

} // namespace

SynthrtEngine::RuntimeOperationLease::RuntimeOperationLease(
    std::shared_lock<std::shared_mutex> lock, srt::core::Runtime *runtime)
    : m_lock(std::move(lock)), m_runtime(runtime) {
}

SynthrtEngine::RuntimeOperationLease::operator bool() const noexcept {
    return m_runtime != nullptr;
}

srt::core::Runtime &SynthrtEngine::RuntimeOperationLease::runtime() const {
    return *m_runtime;
}

// === Singleton ===
SynthrtEngine &SynthrtEngine::instance() {
    auto *engine = AppContext::instance<SynthrtEngine>();
    if (!engine) {
        qFatal("SynthrtEngine::instance() requires an active AppContext");
    }
    return *engine;
}

SynthrtEngine::SynthrtEngine(QObject *parent) : QObject(parent) {
    // Note: log_report_callback is registered by InferEngine constructor,
    // which is the startup entry point and constructs before SynthrtEngine is
    // first accessed. Do not re-register here to avoid overwriting a potential
    // custom callback set by the application.
}

SynthrtEngine::~SynthrtEngine() {
    shutdown();
}

bool SynthrtEngine::initialized() const {
    return m_initialized.load(std::memory_order_acquire);
}

bool SynthrtEngine::runtimeInitialized() const noexcept {
    return m_runtimeInitialized.load(std::memory_order_acquire);
}

bool SynthrtEngine::pitchExtractionReady() const noexcept {
    return m_pitchExtractionReady.load(std::memory_order_acquire);
}

bool SynthrtEngine::midiExtractionReady() const noexcept {
    return m_midiExtractionReady.load(std::memory_order_acquire);
}

bool SynthrtEngine::isAboutToQuit() const noexcept {
    return m_aboutToQuit.load(std::memory_order_acquire);
}

void SynthrtEngine::shutdown() noexcept {
    {
        std::lock_guard stateLock(m_stateMutex);
        m_aboutToQuit.store(true, std::memory_order_release);
        m_initialized.store(false, std::memory_order_release);
        m_runtimeInitialized.store(false, std::memory_order_release);
        m_pitchExtractionReady.store(false, std::memory_order_release);
        m_midiExtractionReady.store(false, std::memory_order_release);
    }
    std::unique_lock lock(m_runtimeLifecycleMutex);
    unloadSinger();
}

fs::path SynthrtEngine::pluginRoot() {
#if defined(Q_OS_MAC)
    return MacOSUtils::getMainBundlePath() / _TSTR("Contents/PlugIns");
#elif defined(Q_OS_WIN)
    return stdc::system::application_directory() / _TSTR("plugins");
#else
    return stdc::system::application_directory().parent_path() / _TSTR("lib/plugins");
#endif
}

SynthrtEngine::RuntimeOperationLease SynthrtEngine::acquirePitchExtractionOperation() {
    std::shared_lock lock(m_runtimeLifecycleMutex);
    if (isAboutToQuit() || !pitchExtractionReady()) {
        return {};
    }
    return {std::move(lock), &m_runtime};
}

SynthrtEngine::RuntimeOperationLease SynthrtEngine::acquireMidiExtractionOperation() {
    std::shared_lock lock(m_runtimeLifecycleMutex);
    if (isAboutToQuit() || !midiExtractionReady()) {
        return {};
    }
    return {std::move(lock), &m_runtime};
}

// === initialize ===
bool SynthrtEngine::initialize(const QStringList &voicebankPaths,
                               const QStringList &g2pPackagePaths, const QString &ep,
                               int deviceIndex) {
    std::unique_lock lock(m_runtimeLifecycleMutex);
    if (isAboutToQuit()) {
        qWarning() << "SynthrtEngine: initialization rejected during shutdown";
        return false;
    }
    if (initialized()) {
        qDebug() << "SynthrtEngine already initialized";
        return true;
    }

    const auto pluginsDir = pluginRoot();

    // Runtime and extraction are independent of package and language readiness.
    if (!m_runtimeInitialized.load(std::memory_order_acquire)) {
        if (!initializeRuntime(pluginsDir, ep, deviceIndex)) {
            return false;
        }
        {
            std::lock_guard stateLock(m_stateMutex);
            if (isAboutToQuit()) {
                return false;
            }
            m_runtimeInitialized.store(true, std::memory_order_release);
        }
        // Plugin discovery is intentionally one-shot for this process/runtime.
        initializeExtractors(pluginsDir);
        if (isAboutToQuit()) {
            return false;
        }
    }

    // --- 1. VoicebankScanner: set search paths ---
    std::vector<fs::path> vbPaths;
    vbPaths.reserve(static_cast<size_t>(voicebankPaths.size()));
    for (const auto &p : std::as_const(voicebankPaths)) {
        vbPaths.emplace_back(StringUtils::qstr_to_path(p));
    }
    // --- 2. Refresh the Lite-owned package catalog ---
    QElapsedTimer timer;
    timer.start();
    auto catalogExp = refreshVoicebanks(vbPaths, true);
    if (!catalogExp) {
        qCritical() << "SynthrtEngine: voicebank scan failed:"
                    << QString::fromUtf8(catalogExp.error().message());
        return false;
    }
    const auto catalog = *catalogExp;
    size_t singerCount = 0;
    for (const auto &package : catalog->packages) {
        singerCount += package.singers.size();
    }
    qDebug() << "Voicebank scan completed in" << timer.elapsed() << "ms;"
             << "singers:" << singerCount << "generation:" << catalog->generation;

    // --- 3. Build packageDirs map for LanguageService ---
    // PackageCatalog validation guarantees package IDs are unambiguous before
    // publication, matching LanguageService's packageId-to-path contract.
    std::unordered_map<std::string, fs::path> pkgDirs;
    for (const auto &package : catalog->packages) {
        if (package.status.valid) {
            pkgDirs.emplace(package.status.packageId, package.status.rootPath);
        }
    }

    // --- 4. Derive G2P plugin paths from the shared plugin root ---
    const auto srtG2pDir = pluginsDir / _TSTR("srt-g2p");
    std::vector<fs::path> g2pPluginPaths;
    g2pPluginPaths.emplace_back(srtG2pDir / _TSTR("G2ps"));
    g2pPluginPaths.emplace_back(srtG2pDir / _TSTR("dict"));

    // --- 5. Register G2P plugin search paths (before ONNX driver init) ---
    // PluginFactory::addPluginPath scans subdirectories for plugin.json and
    // triggers lazy discovery. LanguageService::initialize() will re-register
    // these paths (Stage 1) — PluginFactory deduplicates via
    // scannedPluginDirs, so the re-registration is a safe no-op.
    auto g2pMgr = srt::g2p::Manager::instance();
    for (const auto &path : g2pPluginPaths) {
        g2pMgr->addPluginPath(srt::g2p::kTaskPluginIid, path);
        g2pMgr->addPluginPath(srt::g2p::kDriverPluginIid, path);
    }

    // --- 7. Load G2P ONNX driver (must be before Manager::initialize) ---
    // The ONNX driver is a global infrastructure object (g2pOnnxDriver) that
    // must be registered in the driver category before Manager::initialize()
    // is called (inside LanguageService::initialize Stage 4). Without it,
    // LSTM G2P plugins cannot create ONNX sessions and G2P inference runs in
    // degraded mode.
    // The G2P driver reuses the inference ONNX driver (same plugin) with
    // useCpu forced on every session to avoid GPU contention.
    if (!initializeG2pOnnxDriver()) {
        qWarning() << "SynthrtEngine: G2P ONNX driver not available;"
                      " G2P inference will run in degraded mode";
    }

    // --- 8. LanguageService: initialize G2P (official + voicebank + Manager) ---
    std::vector<fs::path> g2pPathList;
    g2pPathList.reserve(static_cast<size_t>(g2pPackagePaths.size()));
    for (const auto &p : std::as_const(g2pPackagePaths)) {
        g2pPathList.emplace_back(StringUtils::qstr_to_path(p));
    }
    if (auto exp = m_langSvc.initialize(g2pPluginPaths, g2pPathList, pkgDirs); !exp) {
        qCritical() << "SynthrtEngine: LanguageService initialize failed:"
                    << QString::fromUtf8(exp.error().message());
        return false;
    }
    {
        std::lock_guard stateLock(m_stateMutex);
        if (isAboutToQuit()) {
            return false;
        }
        m_initialized.store(true, std::memory_order_release);
    }
    qInfo().noquote() << "Successfully initialized SynthrtEngine. Execution provider:" << ep;
    return true;
}

bool SynthrtEngine::initializeRuntime(const fs::path &pluginRoot, const QString &ep,
                                      int deviceIndex) {
    // Map EP string to OnnxDriverConfig.
    srt::driver::OnnxDriverConfig cfg;
    if (ep == QStringLiteral("DirectML")) {
        cfg.ep = srt::driver::onnx::ExecutionProvider::DMLExecutionProvider;
    } else if (ep == QStringLiteral("CUDA")) {
        cfg.ep = srt::driver::onnx::ExecutionProvider::CUDAExecutionProvider;
    } else if (ep == QStringLiteral("CoreML")) {
        cfg.ep = srt::driver::onnx::ExecutionProvider::CoreMLExecutionProvider;
    } else {
        cfg.ep = srt::driver::onnx::ExecutionProvider::CPUExecutionProvider;
    }
    cfg.deviceIndex = deviceIndex;

    // Validate plugin root exists (equivalent to HEAD initializeSU checkPath).
    if (auto exp = checkPath(pluginRoot); !exp) {
        qCritical() << "SynthrtEngine: invalid plugin root:"
                    << QString::fromUtf8(exp.error().message());
        return false;
    }

    if (auto *plugins = m_runtime.services().get<srt::core::PluginFactory>()) {
        const auto singerProviderDir = pluginRoot / _TSTR("diffsinger/singerproviders");
        const auto inferenceDriverDir = pluginRoot / _TSTR("srt-driver/inferencedrivers");
        const auto interpreterDir = pluginRoot / _TSTR("diffsinger/inferenceinterpreters");
        plugins->addPluginPath("srt.svs.singer-provider.diffsinger", singerProviderDir);
        plugins->addPluginPath("srt.driver.InferenceDriver", inferenceDriverDir);
        plugins->addPluginPath("srt.svs.interpreter.acoustic", interpreterDir);
        plugins->addPluginPath("srt.svs.interpreter.duration", interpreterDir);
        plugins->addPluginPath("srt.svs.interpreter.pitch", interpreterDir);
        plugins->addPluginPath("srt.svs.interpreter.variance", interpreterDir);
        plugins->addPluginPath("srt.svs.interpreter.vocoder", interpreterDir);
    }

    if (auto exp = srt::driver::setupOnnxInferenceDriver(m_runtime, pluginRoot, cfg); !exp) {
        qCritical() << "SynthrtEngine: ONNX driver setup failed:"
                    << QString::fromUtf8(exp.error().message());
        return false;
    }
    return true;
}

void SynthrtEngine::initializeExtractors(const fs::path &pluginRoot) {
    auto *plugins = m_runtime.services().get<srt::core::PluginFactory>();
    if (!plugins) {
        qWarning() << "SynthrtEngine: rmvpe pitch extractor unavailable; PluginFactory is not "
                      "available";
        qWarning() << "SynthrtEngine: game MIDI extractor unavailable; PluginFactory is not "
                      "available";
        return;
    }

    plugins->addPluginPath(srt::extract::kPitchExtractorPluginIid,
                           pluginRoot / _TSTR("srt-extract/PitchExtractor"));
    plugins->addPluginPath(srt::extract::kMidiExtractorPluginIid,
                           pluginRoot / _TSTR("srt-extract/MidiExtractor"));

    const bool hasRmvpe = plugins->plugin<srt::extract::PitchExtractorPlugin>("rmvpe") != nullptr;
    const bool hasGame = plugins->plugin<srt::extract::MidiExtractorPlugin>("game") != nullptr;
    {
        std::lock_guard stateLock(m_stateMutex);
        if (isAboutToQuit()) {
            return;
        }
        m_pitchExtractionReady.store(hasRmvpe, std::memory_order_release);
        m_midiExtractionReady.store(hasGame, std::memory_order_release);
    }

    if (!hasRmvpe) {
        qWarning() << "SynthrtEngine: rmvpe pitch extractor plugin is unavailable";
    }
    if (!hasGame) {
        qWarning() << "SynthrtEngine: game MIDI extractor plugin is unavailable";
    }
}

bool SynthrtEngine::initializeG2pOnnxDriver() {
    const auto mgr = srt::g2p::Manager::instance();

    // Reuse the inference ONNX driver (same plugin, "dsdriver" in the
    // "inference" category) — G2P must not have a separate ONNX driver.
    // The adapter wraps the InferenceDriver and forces useCpu=true on every
    // session open() so G2P never competes with GPU inference.
    auto *inferenceCat = m_runtime.moduleCategory("inference");
    if (!inferenceCat) {
        qWarning() << "SynthrtEngine: inference module category not found";
        return false;
    }

    const auto driverObj = inferenceCat->getFirstObject("dsdriver");
    if (!driverObj) {
        qWarning() << "SynthrtEngine: inference ONNX driver 'dsdriver' not found";
        return false;
    }

    const auto onnxDriver = driverObj.as<srt::driver::InferenceDriver>();
    if (!onnxDriver) {
        qWarning() << "SynthrtEngine: inference 'dsdriver' is not an InferenceDriver";
        return false;
    }

    const auto factory = srt::core::NO<G2pOnnxSessionFactory>::create(onnxDriver);

    auto *driverCategory = mgr->category(srt::g2p::kDriverCategory);
    if (!driverCategory) {
        qWarning() << "SynthrtEngine: G2P driver category not found";
        return false;
    }
    driverCategory->addObject(srt::g2p::kG2pOnnxDriverName, factory);
    qDebug() << "SynthrtEngine: G2P ONNX driver loaded successfully"
                " (CPU-only adapter over inference driver)";
    return true;
}

// === refreshVoicebanks ===
srt::core::Expected<std::shared_ptr<const PackageCatalog::Snapshot>>
    SynthrtEngine::refreshVoicebanks(const std::vector<std::filesystem::path> &searchPaths,
                                     bool allowReuse) {
    std::scoped_lock refreshLock(m_catalogRefreshMutex);
    auto candidate = m_catalog.prepareRefresh(searchPaths, allowReuse || !initialized());
    if (!candidate) {
        return candidate.error();
    }
    if (candidate->unchanged()) {
        return m_catalog.commit(std::move(*candidate));
    }

    decltype(m_singerSessions) staleSessions;
    QWriteLocker singerLock(&m_singerRwLock);
    const auto accepted = m_catalog.snapshot();
    if (accepted->generation != 0 &&
        candidate->snapshot().languageFingerprint != accepted->languageFingerprint) {
        return srt::core::Error(
            srt::core::ErrorCode::G2pInitializationError,
            "Package refresh changes language package roots or metadata; the pinned synthrt "
            "LanguageService cannot reload safely, so the accepted catalog was preserved");
    }
    if (!m_loadedPackageDirs.empty() &&
        candidate->snapshot().catalogFingerprint != accepted->catalogFingerprint) {
        return srt::core::Error(
            srt::core::ErrorCode::PackageScanAfterInitialize,
            "Package refresh changes voicebank metadata after packages were loaded into synthrt "
            "Runtime; restart the application to reload packages safely");
    }

    auto committed = m_catalog.commit(std::move(*candidate));
    if (!committed) {
        return committed.error();
    }
    staleSessions.swap(m_singerSessions);
    singerLock.unlock();
    return committed;
}

srt::core::Expected<ds::bank::SingerSnapshot>
    SynthrtEngine::singerSnapshot(const SingerIdentifier &identifier) const {
    const auto catalog = m_catalog.snapshot();
    if (const auto *singer = catalog->findSinger(identifier)) {
        return *singer;
    }
    return srt::core::Error(srt::core::ErrorCode::SvsSingerNotFound,
                            "Singer not found in package catalog");
}

srt::core::Expected<SingerIdentifier> SynthrtEngine::findSinger(const QString &singerId) const {
    const auto catalog = m_catalog.snapshot();
    const ds::bank::SingerSnapshot *match = nullptr;
    for (const auto &package : catalog->packages) {
        for (const auto &singer : package.singers) {
            if (singer.ref.singerId != singerId.toStdString()) {
                continue;
            }
            if (match) {
                return srt::core::Error(srt::core::ErrorCode::PackageVersionConflict,
                                        "Singer ID is ambiguous across catalog packages");
            }
            match = &singer;
        }
    }
    if (!match) {
        return srt::core::Error(srt::core::ErrorCode::SvsSingerNotFound,
                                "Singer not found in package catalog");
    }
    const auto &ref = match->ref;
    SingerIdentifier id;
    id.singerId = QString::fromUtf8(ref.singerId);
    id.packageId = QString::fromUtf8(ref.packageId);
    // v2: SingerRef carries the manifest version string; propagate it so that
    // resolve(runtime, packageId, singerId, version) can locate the singer
    // precisely across multiple package versions.
    if (!ref.version.empty()) {
        id.packageVersion = QVersionNumber::fromString(QString::fromUtf8(ref.version));
    }
    return id;
}

std::filesystem::path SynthrtEngine::packageDirectory(const SingerIdentifier &identifier) const {
    const auto catalog = m_catalog.snapshot();
    const auto *package = catalog->findPackage(identifier);
    return package ? package->status.rootPath : std::filesystem::path{};
}

std::shared_ptr<SingerModelSession>
    SynthrtEngine::acquireSingerSession(const SingerIdentifier &identifier) {
    if (!initialized()) {
        qCritical() << "SynthrtEngine::loadSinger: engine not initialized";
        return {};
    }
    if (isAboutToQuit()) {
        return {};
    }

    QWriteLocker lock(&m_singerRwLock);
    if (isAboutToQuit()) {
        return {};
    }
    if (const auto it = m_singerSessions.find(identifier); it != m_singerSessions.end()) {
        return it->second;
    }

    const auto packageId = identifier.packageId.toStdString();
    const auto singerId = identifier.singerId.toStdString();
    // v2: pass the manifest version string for precise singer lookup across
    // multiple versions of the same package.
    const auto versionStr = identifier.packageVersion.isNull()
                                ? std::string{}
                                : identifier.packageVersion.toString().toStdString();

    // --- 1. Load package in Runtime (v2: loadPackage replaces open) ---
    auto pkgDir = packageDirectory(identifier);
    if (pkgDir.empty()) {
        qCritical().noquote().nospace()
            << "SynthrtEngine: package directory not found for " << identifier;
        return {};
    }
    const auto loadedPkgDir = stablePackagePath(pkgDir);
    if (!m_loadedPackageDirs.contains(loadedPkgDir)) {
        if (auto exp = m_runtime.loadPackage(pkgDir); !exp) {
            qCritical().noquote().nospace()
                << "SynthrtEngine: failed to load package \"" << identifier.packageId
                << "\": " << QString::fromUtf8(exp.error().message());
            return {};
        }
        m_loadedPackageDirs.insert(loadedPkgDir);
    }

    // --- 2. Resolve 5 StageSpecs via SingerStageResolver (v2: with version) ---
    ds::infer::SingerStageResolver resolver;
    auto stagesExp = resolver.resolve(m_runtime, packageId, singerId, versionStr);
    if (!stagesExp) {
        qCritical().noquote().nospace()
            << "SynthrtEngine: failed to resolve stages for singer \"" << identifier.singerId
            << "\": " << QString::fromUtf8(stagesExp.error().message());
        return {};
    }

    // --- 3. Construct ModelSet from the resolved StageSet (lazy: no Inference
    //     objects are created until load(kind) is called by a task). ---
    auto session = std::make_shared<SingerModelSession>(identifier,
                                                        ds::infer::ModelSet(std::move(*stagesExp)));
    if (isAboutToQuit()) {
        return {};
    }
    m_singerSessions.emplace(identifier, session);
    lock.unlock();
    Q_EMIT singerLoaded(identifier);
    qInfo() << "SynthrtEngine: singer loaded (lazy, models not yet created)" << identifier;
    return session;
}

// === unloadSinger — v2 ===
void SynthrtEngine::unloadSinger() {
    decltype(m_singerSessions) sessions;
    {
        QWriteLocker lock(&m_singerRwLock);
        sessions.swap(m_singerSessions);
    }
}

// === Language service ===
namespace {
    std::string toUtf8(const QString &value) {
        const auto bytes = value.toUtf8();
        return {bytes.constData(), static_cast<size_t>(bytes.size())};
    }
}

srt::core::Expected<srt::g2p::LanguageRoute>
    SynthrtEngine::resolveLanguageRoute(const SingerIdentifier &identifier,
                                        const QString &languageId) const {
    const auto packageId = toUtf8(identifier.packageId);
    const auto singerId = toUtf8(identifier.singerId);
    const auto lang = toUtf8(languageId);
    return m_langSvc.resolveLanguageRoute(packageId, singerId, lang);
}

const srt::g2p::LanguageService &SynthrtEngine::languageService() const noexcept {
    return m_langSvc;
}

srt::core::Expected<std::shared_ptr<srt::s2p::LanguageResource>>
    SynthrtEngine::resolveS2pResource(const SingerIdentifier &identifier,
                                      const QString &languageId) const {
    const auto packageId = toUtf8(identifier.packageId);
    const auto singerId = toUtf8(identifier.singerId);
    const auto lang = toUtf8(languageId);
    return m_langSvc.resolveS2pResource(packageId, singerId, lang);
}

// === Runtime access ===
srt::core::Runtime &SynthrtEngine::runtime() {
    return m_runtime;
}

const srt::core::Runtime &SynthrtEngine::runtime() const {
    return m_runtime;
}
