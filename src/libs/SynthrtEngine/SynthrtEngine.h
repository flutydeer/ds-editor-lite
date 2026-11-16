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

/// Owns the shared runtime and provides access to the current voicebank snapshot.
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
    // voicebankPaths: directories containing voicebank packages (desc.json)
    // g2pPackagePaths: official G2P package paths
    // ep: ONNX execution provider ("CPU", "DirectML", "CUDA", or "CoreML")
    // deviceIndex: GPU device index (ignored for CPU)
    // deferLanguageModels: load G2P models on warm-up or when the session first needs them.
    bool initialize(const QStringList &voicebankPaths, const QStringList &g2pPackagePaths,
                    const QString &ep = QStringLiteral("CPU"), int deviceIndex = 0,
                    bool deferLanguageModels = false);

    bool initialized() const;
    /// True when initialization finishes, including rejected host prerequisites or shutdown.
    bool initializationDone() const noexcept;
    /// Releases initialization waiters when the host cannot start the shared runtime.
    void completeInitializationAttempt() noexcept;
    /// Wait for initialization to finish. Returns false if \p timeoutMs elapses.
    /// Check initialized() for success. Multiple threads may wait concurrently.
    bool waitForInitialization(int timeoutMs = 30000) const;
    /// True once the voicebank scan and language metadata are available.
    /// Snapshot consumers can query the session while G2P models load.
    bool sessionReady() const noexcept;
    /// Block until the session is ready or initialization finishes, fails, or is shut down.
    /// Returns false on timeout. After a true result, check sessionReady().
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
    // Re-scan voicebank directories and retain per-package diagnostics.
    // VoicebankSession handles voicebank scanning, LanguageService metadata
    // update, and atomic snapshot publication internally.
    srt::core::Expected<ds::session::RefreshResult>
        refreshVoicebanks(const std::vector<std::filesystem::path> &searchPaths);

    /// Cached singer snapshot from the current VoicebankSession snapshot.
    srt::core::Expected<ds::bank::SingerSnapshot>
        singerSnapshot(const SingerIdentifier &identifier) const;

    /// Lookup SingerIdentifier by singerId (scans all packages).
    srt::core::Expected<SingerIdentifier> findSinger(const QString &singerId) const;

    /// Exact package directory for a versioned singer identifier.
    std::filesystem::path packageDirectory(const SingerIdentifier &identifier) const;

    // Access version-aware routing and loading of voicebank resources.
    ds::session::VoicebankSession &session();
    const ds::session::VoicebankSession &session() const;

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

    // Load deferred G2P models ahead of the first conversion. This call is idempotent and
    // thread-safe. It can block on model loading, so schedule warm-up on a background thread.
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

    // Wakes waiters when m_initializationDone is published.
    mutable std::mutex m_initDoneMutex;
    mutable std::condition_variable m_initDoneCv;
    // Wakes waiters when the session becomes ready or initialization ends.
    mutable std::mutex m_sessionReadyMutex;
    mutable std::condition_variable m_sessionReadyCv;

    // Declare the resources before the session so both outlive it.
    std::shared_ptr<srt::g2p::LanguageService> m_langSvc =
        std::make_shared<srt::g2p::LanguageService>();
    srt::core::Runtime m_runtime;
    // initialize() supplies the session's resources. The session synchronizes its own refreshes.
    ds::session::VoicebankSession m_session;
    // Release/acquire publishes session readiness to snapshot readers.
    std::atomic<bool> m_sessionInitialized{false};

    mutable std::mutex m_stateMutex;
    mutable std::shared_mutex m_runtimeLifecycleMutex;

    // Internal helpers
    bool initializeRuntime(const std::filesystem::path &pluginRoot, const QString &ep,
                           int deviceIndex);
    void initializeExtractors(const std::filesystem::path &pluginRoot);
    bool initializeG2pOnnxDriver();
};

#endif // SYNTHRT_ENGINE_H
