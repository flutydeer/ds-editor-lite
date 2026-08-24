//
// SynthrtEngine - v2 component facade.
//
// Combines the independent synthrt v2 components (VoicebankSession,
// LanguageService, Runtime + ONNX driver) into a single facade that the lite
// module layer (PackageManager / InferEngine / Language modules) delegates to.
//
// VoicebankSession is the synthrt v2 chokepoint for voicebank scanning,
// language metadata, ensureModelSet / convertG2p / convertS2p. SynthrtEngine
// owns the Runtime and LanguageService lifetimes and borrows them to the
// session via SessionResources.
//
// v3 changes vs v2:
//   - Removed PackageCatalog: VoicebankSession.snapshot() is the single source
//     of truth for package/singer/manifest queries.
//   - Removed SingerModelSession: InferEngine acquires ModelSetHandle directly
//     via VoicebankSession::ensureModelSet().
//   - VoicebankSession.refresh() handles voicebank scanning + LanguageService
//     metadata initialization in one call.
//
// Design reference: docs/design/design-guidelines.md (ARCH-03, ARCH-04)
//

#ifndef SYNTHRT_ENGINE_H
#define SYNTHRT_ENGINE_H

#include <atomic>
#include <condition_variable>
#include <memory>
#include <filesystem>
#include <shared_mutex>
#include <vector>

#include <QObject>
#include <QString>
#include <QStringList>

#include <synthrt/Core/Core/Runtime.h>
#include <synthrt/Core/Support/Expected.h>
#include <synthrt/SVS/Inference.h>
#include <synthrt/SVS/InferenceContrib.h>

#include <synthrt/G2P/LanguageService.h>
#include <diffsinger/Session/VoicebankSession.h>
#include <synthrt/Driver/OnnxSetup.h>

#include <lite/ProjectModel/AppModel/SingerIdentifier.h>

class SynthrtEngine final : public QObject {
    Q_OBJECT

private:
    friend class SingletonRegistry; // constructed/destroyed via SingletonRegistry::create/destroy
    explicit SynthrtEngine(QObject *parent = nullptr);
    ~SynthrtEngine() override;

public:
    class RuntimeOperationLease {
    public:
        RuntimeOperationLease() = default;
        RuntimeOperationLease(RuntimeOperationLease &&) noexcept = default;
        RuntimeOperationLease &operator=(RuntimeOperationLease &&) noexcept = default;

        Q_DISABLE_COPY(RuntimeOperationLease)

        explicit operator bool() const noexcept;
        srt::core::Runtime &runtime() const;

    private:
        friend class SynthrtEngine;
        RuntimeOperationLease(std::shared_lock<std::shared_mutex> lock,
                              srt::core::Runtime *runtime);

        std::shared_lock<std::shared_mutex> m_lock;
        srt::core::Runtime *m_runtime = nullptr;
    };

    static SynthrtEngine &instance();

    Q_DISABLE_COPY_MOVE(SynthrtEngine)

    // === Initialization (call once at startup) ===
    //
    // Sets up Runtime and extractor discovery before the package catalog and
    // LanguageService. Extraction therefore remains available if package or
    // language initialization fails.
    //
    // voicebankPaths  — directories containing voicebank packages (desc.json)
    // g2pPackagePaths — official G2P package paths
    // ep              — ONNX execution provider ("CPU" / "DirectML" / "CUDA" / "CoreML")
    // deviceIndex     — GPU device index (ignored for CPU)
    // deferLanguageModels — if true, skip Stage 2 (loading all G2P ONNX
    //   sessions) during startup. The first actual G2P conversion then loads
    //   models lazily via LanguageService::initializeModels() (idempotent,
    //   guarded by Manager's internal mutex), triggered from
    //   VoicebankSession::ensureLanguageReady() on first use.
    bool initialize(const QStringList &voicebankPaths, const QStringList &g2pPackagePaths,
                    const QString &ep = QStringLiteral("CPU"), int deviceIndex = 0,
                    bool deferLanguageModels = false);

    bool initialized() const;
    /// True after initialize() has been attempted (regardless of success).
    /// Callers that depend on SynthrtEngine being ready can poll this to detect
    /// initialization failure without waiting forever on initialized().
    bool initializationDone() const noexcept;
    /// Block until initialize() has been attempted (success or failure), or
    /// \p timeoutMs elapses. Returns true if initialization finished within
    /// the timeout (check initialized() afterwards to see if it succeeded),
    /// false on timeout. Uses a condition variable internally — no polling.
    /// Safe to call from multiple threads concurrently.
    bool waitForInitialization(int timeoutMs = 30000) const;
    /// True once VoicebankSession has completed Stage 1 (voicebank scan +
    /// LanguageService metadata). PackageManager and other snapshot consumers
    /// can query VoicebankSnapshot via refreshVoicebanks() / singerSnapshot()
    /// once this returns true, even while Stage 2 (ONNX model loading) is
    /// still in progress.
    bool sessionReady() const noexcept;
    /// Block until sessionReady() becomes true or \p timeoutMs elapses.
    /// Returns true if the session became ready within the timeout, false on
    /// timeout. Uses a condition variable internally — no polling.
    bool waitForSession(int timeoutMs = 30000) const;
    bool runtimeInitialized() const noexcept;
    bool pitchExtractionReady() const noexcept;
    bool midiExtractionReady() const noexcept;
    bool isAboutToQuit() const noexcept;
    void shutdown() noexcept;

    static std::filesystem::path pluginRoot();

    [[nodiscard]] RuntimeOperationLease acquirePitchExtractionOperation();
    [[nodiscard]] RuntimeOperationLease acquireMidiExtractionOperation();

    // === Voicebank snapshot (delegates to VoicebankSession) ===
    //
    // Re-scan voicebank directories. Returns the new snapshot on success.
    // VoicebankSession handles voicebank scanning, LanguageService metadata
    // update, and atomic snapshot publication internally.
    srt::core::Expected<std::shared_ptr<const ds::session::VoicebankSnapshot>>
        refreshVoicebanks(const std::vector<std::filesystem::path> &searchPaths,
                          bool allowReuse = true);

    /// Cached singer snapshot from the current VoicebankSession snapshot.
    srt::core::Expected<ds::bank::SingerSnapshot>
        singerSnapshot(const SingerIdentifier &identifier) const;

    /// Lookup SingerIdentifier by singerId (scans all packages).
    srt::core::Expected<SingerIdentifier> findSinger(const QString &singerId) const;

    /// Exact package directory for a versioned singer identifier.
    std::filesystem::path packageDirectory(const SingerIdentifier &identifier) const;

    // === VoicebankSession (synthrt v2 chokepoint) ===
    //
    // VoicebankSession provides ensureModelSet() / ensureLanguageReady() /
    // convertG2p() / convertS2p() with version-aware routing. InferEngine,
    // G2pService and the DiffSinger S2P/G2P tasks delegate to session().
    // The session borrows m_runtime and m_langSvc via SessionResources;
    // SynthrtEngine owns their lifetime.
    ds::session::VoicebankSession &session();
    const ds::session::VoicebankSession &session() const;

    // === Language service (replaces G2pConvertRunner / S2pMgr / OnsetMarkerMgr) ===
    //
    // Resolve the language route (G2P context + S2P resource + onset) for a
    // singer + language combination.
    srt::core::Expected<srt::g2p::LanguageRoute>
        resolveLanguageRoute(const SingerIdentifier &identifier, const QString &languageId) const;

    const srt::g2p::LanguageService &languageService() const noexcept;

    // Resolve the S2P LanguageResource independently (cached). Use this when
    // the user edits pronunciation after G2P has already run and only S2P
    // needs to re-execute. Returns shared_ptr so callers can call convert()
    // directly without managing resource lifetime.
    srt::core::Expected<std::shared_ptr<srt::s2p::LanguageResource>>
        resolveS2pResource(const SingerIdentifier &identifier, const QString &languageId) const;

    // Ensure G2P language models are loaded (idempotent, thread-safe via
    // synthrt's Manager init lock). When called after initialize() ran with
    // deferLanguageModels=true, this kicks off the deferred Stage 2 load so
    // the first G2P conversion does not stall the calling thread. Intended to
    // be invoked from a background thread (e.g. QThreadPool::globalInstance())
    // right after startup so the FillLyric dialog and inference tasks find
    // models ready.
    bool warmUpLanguageModels();

    // === Runtime access ===
    srt::core::Runtime &runtime();
    const srt::core::Runtime &runtime() const;

private:
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_initializationDone{false};
    std::atomic<bool> m_runtimeInitialized{false};
    std::atomic<bool> m_pitchExtractionReady{false};
    std::atomic<bool> m_midiExtractionReady{false};
    std::atomic<bool> m_aboutToQuit{false};

    // Signaled when initialize() finishes (success or failure). Paired with
    // m_initializationDone as the predicate. PackageManager and other consumers
    // wait on this instead of polling.
    mutable std::mutex m_initDoneMutex;
    mutable std::condition_variable m_initDoneCv;
    // Signaled when VoicebankSession becomes ready (Stage 1 complete). Paired
    // with m_sessionInitialized. PackageManager waits on this so it can query
    // the snapshot without blocking on Stage 2 (ONNX model loading).
    mutable std::mutex m_sessionReadyMutex;
    mutable std::condition_variable m_sessionReadyCv;

    // Components (v3 architecture: VoicebankSession is the single chokepoint)
    // Shared_ptr so SessionResources can borrow the LanguageService without
    // extending its lifetime (SynthrtEngine owns both m_langSvc and m_session;
    // the session does not outlive the engine).
    std::shared_ptr<srt::g2p::LanguageService> m_langSvc =
        std::make_shared<srt::g2p::LanguageService>();
    srt::core::Runtime m_runtime;
    // VoicebankSession: the synthrt v2 chokepoint for voicebank scanning,
    // ensureModelSet / convertG2p / convertS2p / ensureLanguageReady.
    // Default-constructed until initialize() move-assigns a resource-injected
    // instance. VoicebankSession handles internal locking; no external mutex
    // is needed for refreshVoicebanks().
    ds::session::VoicebankSession m_session;
    bool m_sessionInitialized = false;

    mutable std::mutex m_stateMutex;
    mutable std::shared_mutex m_runtimeLifecycleMutex;

    // Internal helpers
    bool initializeRuntime(const std::filesystem::path &pluginRoot, const QString &ep,
                           int deviceIndex);
    void initializeExtractors(const std::filesystem::path &pluginRoot);
    bool initializeG2pOnnxDriver();
};

#endif // SYNTHRT_ENGINE_H
