//
// SynthrtEngine implementation — v2 component facade.
//
// Reference: docs/refactoring-v2/03-lite-integration.md
//

#include "SynthrtEngine.h"

#include <lite/Core/SingletonRegistry.h>
#include <lite/Support/StringUtils.h>
#include <lite/Support/VersionUtils.h>

#include <stdcorelib/path.h>
#include <stdcorelib/support/versionnumber.h>
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

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <cwctype>
#include <thread>
#include <vector>

#if defined(Q_OS_MAC)
#  include <lite/Support/MacOSUtils.h>
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
    auto *engine = SingletonRegistry::instance<SynthrtEngine>();
    if (!engine) {
        qFatal("SynthrtEngine::instance() requires an owner to register it "
               "(e.g. the app's AppContext)");
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

bool SynthrtEngine::initializationDone() const noexcept {
    return m_initializationDone.load(std::memory_order_acquire);
}

bool SynthrtEngine::waitForInitialization(int timeoutMs) const {
    if (m_initializationDone.load(std::memory_order_acquire)) {
        return true;
    }
    std::unique_lock lock(m_initDoneMutex);
    return m_initDoneCv.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this] {
        return m_initializationDone.load(std::memory_order_acquire);
    });
}

bool SynthrtEngine::sessionReady() const noexcept {
    return m_sessionInitialized;
}

bool SynthrtEngine::waitForSession(int timeoutMs) const {
    // Fast path: session already ready.
    if (m_sessionInitialized) {
        return true;
    }
    // Wait until either the session becomes ready, or initialize() finishes
    // (success or failure). If initialize() finished without setting
    // m_sessionInitialized, Stage 1 failed and the caller should surface the
    // refreshVoicebanks() error rather than wait out the full timeout.
    std::unique_lock lock(m_sessionReadyMutex);
    return m_sessionReadyCv.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this] {
        return m_sessionInitialized || m_initializationDone.load(std::memory_order_acquire);
    });
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
        m_sessionInitialized = false;
    }
    std::unique_lock lock(m_runtimeLifecycleMutex);
    // VoicebankSession destructor handles cleanup of loaded packages and
    // ModelSet handles. No explicit unloadSinger() needed — active inference
    // tasks hold shared_ptr<ModelSetHandle> which keep the session alive
    // until they complete.
}

fs::path SynthrtEngine::pluginRoot() {
#if defined(Q_OS_MAC)
    return MacOSUtils::getMainBundlePath() / "Contents/PlugIns";
#elif defined(Q_OS_WIN)
    return stdc::system::application_directory() / "plugins";
#else
    return stdc::system::application_directory().parent_path() / "lib/plugins";
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
        m_initializationDone.store(true, std::memory_order_release);
        {
            std::lock_guard lk(m_initDoneMutex);
        }
        m_initDoneCv.notify_all();
        return false;
    }
    if (initialized()) {
        qDebug() << "SynthrtEngine already initialized";
        return true;
    }

    // RAII: mark initialization as done on any return path (success or failure)
    // and notify any waiters on m_initDoneCv and m_sessionReadyCv so they don't
    // block forever. Callers waiting on sessionReady() must check the returned
    // snapshot error to detect that initialization failed before Stage 1.
    struct InitDoneGuard {
        SynthrtEngine &engine;

        ~InitDoneGuard() {
            engine.m_initializationDone.store(true, std::memory_order_release);
            {
                std::lock_guard lk(engine.m_initDoneMutex);
                std::lock_guard lk2(engine.m_sessionReadyMutex);
            }
            engine.m_initDoneCv.notify_all();
            engine.m_sessionReadyCv.notify_all();
        }
    } initDoneGuard{*this};

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

    // --- 1. Derive voicebank search paths ---
    std::vector<fs::path> vbPaths;
    vbPaths.reserve(static_cast<size_t>(voicebankPaths.size()));
    for (const auto &p : std::as_const(voicebankPaths)) {
        vbPaths.emplace_back(StringUtils::qstr_to_path(p));
    }

    // --- 2. Derive G2P plugin paths from the shared plugin root ---
    const auto srtG2pDir = pluginsDir / "srt-g2p";
    std::vector<fs::path> g2pPluginPaths;
    g2pPluginPaths.emplace_back(srtG2pDir / "G2ps");
    g2pPluginPaths.emplace_back(srtG2pDir / "dict");

    // --- 3. Register G2P plugin search paths (before ONNX driver init) ---
    // PluginFactory::addPluginPath scans subdirectories for plugin.json and
    // triggers lazy discovery. LanguageService::initializeMetadata() (called
    // by VoicebankSession::refresh() below) will re-register these paths
    // (Stage 1); PluginFactory deduplicates via scannedPluginDirs, so the
    // re-registration is a safe no-op.
    auto g2pMgr = srt::g2p::Manager::instance();
    for (const auto &path : g2pPluginPaths) {
        g2pMgr->addPluginPath(srt::g2p::kTaskPluginIid, path);
        g2pMgr->addPluginPath(srt::g2p::kDriverPluginIid, path);
    }

    // --- 4. Load G2P ONNX driver (must be before Manager::initialize) ---
    // The ONNX driver is a global infrastructure object (g2pOnnxDriver) that
    // must be registered in the driver category before Manager::initialize()
    // is called (inside LanguageService::initializeMetadata Stage 4, which
    // VoicebankSession::refresh() triggers). Without it, LSTM G2P plugins
    // cannot create ONNX sessions and G2P inference runs in degraded mode.
    // The G2P driver reuses the inference ONNX driver (same plugin) with
    // useCpu forced on every session to avoid GPU contention.
    if (!initializeG2pOnnxDriver()) {
        qWarning() << "SynthrtEngine: G2P ONNX driver not available;"
                      " G2P inference will run in degraded mode";
    }

    // --- 5. Build official G2P package paths ---
    std::vector<fs::path> officialG2pPackages;
    officialG2pPackages.reserve(static_cast<size_t>(g2pPackagePaths.size()));
    for (const auto &p : std::as_const(g2pPackagePaths)) {
        officialG2pPackages.emplace_back(StringUtils::qstr_to_path(p));
    }

    // --- 6. VoicebankSession: resource-inject Runtime + LanguageService ---
    // VoicebankSession::refresh() does voicebank scanning + LanguageService
    // metadata initialization in one call. The session borrows m_runtime and
    // m_langSvc via references; SynthrtEngine outlives both.
    ds::session::SessionResources resources;
    resources.runtime = &m_runtime;
    resources.languageService = m_langSvc;
    resources.g2pPluginPaths = std::move(g2pPluginPaths);
    resources.officialG2pPackages = std::move(officialG2pPackages);
    m_session = ds::session::VoicebankSession(std::move(resources));
    m_session.setRoots(vbPaths);

    // --- 7. Refresh: scan voicebanks + initialize LanguageService metadata ---
    QElapsedTimer timer;
    timer.start();
    auto refreshResult = m_session.refresh();
    if (!refreshResult.succeeded) {
        qCritical() << "SynthrtEngine: VoicebankSession refresh failed:"
                    << QString::fromStdString(refreshResult.errorMessage);
        return false;
    }
    const auto snapshot = refreshResult.snapshot;
    const size_t singerCount = snapshot->singers.size();
    qDebug() << "Voicebank scan completed in" << timer.elapsed() << "ms;"
             << "singers:" << singerCount << "generation:" << snapshot->generation;
    if (!refreshResult.languageReady) {
        qWarning() << "SynthrtEngine: VoicebankSession reports language module not ready";
    }

    // VoicebankSession is now ready (Stage 1 complete): snapshot is published
    // and LanguageService metadata is initialized. PackageManager and other
    // snapshot consumers can query refreshVoicebanks() / singerSnapshot() from
    // this point onward, even while Stage 2 (ONNX model loading) is still in
    // progress below.
    m_sessionInitialized = true;
    {
        std::lock_guard lk(m_sessionReadyMutex);
    }
    m_sessionReadyCv.notify_all();

    // --- 8. LanguageService: initialize models (Stage 2, loads ONNX DLLs) ---
    // VoicebankSession::refresh() only calls initializeMetadata() (Stage 1).
    // Stage 2 loads G2P plugin DLLs and creates ONNX sessions; must be called
    // separately by the host.
    if (auto exp = m_langSvc->initializeModels(); !exp) {
        qCritical() << "SynthrtEngine: LanguageService initializeModels failed:"
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
        const auto singerProviderDir = pluginRoot / "diffsinger/singerproviders";
        const auto inferenceDriverDir = pluginRoot / "srt-driver/inferencedrivers";
        const auto interpreterDir = pluginRoot / "diffsinger/inferenceinterpreters";
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
                           pluginRoot / "srt-extract/PitchExtractor");
    plugins->addPluginPath(srt::extract::kMidiExtractorPluginIid,
                           pluginRoot / "srt-extract/MidiExtractor");

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
srt::core::Expected<std::shared_ptr<const ds::session::VoicebankSnapshot>>
    SynthrtEngine::refreshVoicebanks(const std::vector<std::filesystem::path> &searchPaths,
                                     bool allowReuse) {
    if (!m_sessionInitialized) {
        return srt::core::Error(srt::core::ErrorCode::InferenceNotInitialized,
                                "SynthrtEngine::refreshVoicebanks: session not initialized");
    }
    // VoicebankSession handles internal locking; concurrent callers share the
    // in-flight refresh operation. allowReuse is honored by skipping the
    // refresh when searchPaths match the current roots and the caller allows it.
    if (allowReuse) {
        const auto current = m_session.snapshot();
        const auto &roots = m_session.roots();
        if (current && current->generation != 0 && roots == searchPaths) {
            return current;
        }
    }
    m_session.setRoots(searchPaths);
    auto result = m_session.refresh();
    if (!result.succeeded) {
        return srt::core::Error(srt::core::ErrorCode::PackageScanAfterInitialize,
                                result.errorMessage);
    }
    return result.snapshot;
}

srt::core::Expected<ds::bank::SingerSnapshot>
    SynthrtEngine::singerSnapshot(const SingerIdentifier &identifier) const {
    const auto snapshot = m_session.snapshot();
    if (!snapshot) {
        return srt::core::Error(srt::core::ErrorCode::InferenceNotInitialized,
                                "VoicebankSession snapshot not available");
    }
    // SingerIdentifier has an implicit conversion to ds::bank::SingerRef.
    if (const auto *singer = snapshot->findSinger(identifier)) {
        return *singer;
    }
    return srt::core::Error(srt::core::ErrorCode::SvsSingerNotFound,
                            "Singer not found in voicebank snapshot");
}

srt::core::Expected<SingerIdentifier> SynthrtEngine::findSinger(const QString &singerId) const {
    const auto snapshot = m_session.snapshot();
    if (!snapshot) {
        return srt::core::Error(srt::core::ErrorCode::InferenceNotInitialized,
                                "VoicebankSession snapshot not available");
    }
    const auto matches = snapshot->findSingersBySingerId(singerId.toStdString());
    if (matches.empty()) {
        return srt::core::Error(srt::core::ErrorCode::SvsSingerNotFound,
                                "Singer not found in voicebank snapshot");
    }
    if (matches.size() > 1) {
        return srt::core::Error(srt::core::ErrorCode::PackageVersionConflict,
                                "Singer ID is ambiguous across catalog packages");
    }
    const auto &ref = matches[0]->ref;
    SingerIdentifier id;
    id.singerId = QString::fromUtf8(ref.singerId);
    id.packageId = QString::fromUtf8(ref.packageId);
    if (!ref.version.empty()) {
        id.packageVersion = QVersionNumber::fromString(QString::fromUtf8(ref.version));
    }
    return id;
}

std::filesystem::path SynthrtEngine::packageDirectory(const SingerIdentifier &identifier) const {
    const auto snapshot = m_session.snapshot();
    if (!snapshot) {
        return {};
    }
    const auto version = VersionUtils::qt_to_stdc(identifier.packageVersion);
    const auto *package = snapshot->findPackage(identifier.packageId.toStdString(), version);
    return package ? package->rootPath : std::filesystem::path{};
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
    // Level=3: pass the real voicebank package version for precise route
    // resolution. An empty version would cause G2pVersionAmbiguous when
    // multiple versions of the same packageId exist.
    const auto version = VersionUtils::qt_to_stdc(identifier.packageVersion);
    return m_langSvc->resolveLanguageRoute(packageId, version, singerId, lang);
}

const srt::g2p::LanguageService &SynthrtEngine::languageService() const noexcept {
    return *m_langSvc;
}

srt::core::Expected<std::shared_ptr<srt::s2p::LanguageResource>>
    SynthrtEngine::resolveS2pResource(const SingerIdentifier &identifier,
                                      const QString &languageId) const {
    const auto packageId = toUtf8(identifier.packageId);
    const auto singerId = toUtf8(identifier.singerId);
    const auto lang = toUtf8(languageId);
    // Level=3: pass the real voicebank package version for precise S2P
    // resource resolution (independent cache slot per version).
    const auto version = VersionUtils::qt_to_stdc(identifier.packageVersion);
    return m_langSvc->resolveS2pResource(packageId, version, singerId, lang);
}

// === Runtime access ===
srt::core::Runtime &SynthrtEngine::runtime() {
    return m_runtime;
}

const srt::core::Runtime &SynthrtEngine::runtime() const {
    return m_runtime;
}

// === VoicebankSession (B1b) ===
ds::session::VoicebankSession &SynthrtEngine::session() {
    return m_session;
}

const ds::session::VoicebankSession &SynthrtEngine::session() const {
    return m_session;
}
