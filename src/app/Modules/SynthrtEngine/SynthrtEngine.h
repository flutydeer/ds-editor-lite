//
// SynthrtEngine - v2 component facade.
//
// Combines the independent synthrt v2 components (VoicebankScanner,
// LanguageService, Runtime + ONNX driver, SingerStageResolver, ModelSet) into
// a single facade that the lite module layer (PackageManager / InferEngine /
// Language modules) delegates to.
//
// SynthrtEngine aggregates the synthrt components and owns singer-specific model
// sessions used by the inference pipeline.
//
// v2 changes vs v1:
//   - The 5 `NO<Inference>` members are replaced by a single
//     `ds::infer::ModelSet`, which performs lazy per-stage create + initialize
//     on `load(kind)` and supports independent `unload(kind)`.
//   - Singer ModelSets are lazy and retained independently by SingerIdentifier.
//   - `Runtime::loadPackage()` replaces `Runtime::open()`.
//   - `SingerStageResolver::resolve()` takes a version string for precise
//     singer lookup.
//
// Design reference: docs/refactoring-v2/03-lite-integration.md
//

#ifndef SYNTHRT_ENGINE_H
#define SYNTHRT_ENGINE_H

#include <atomic>
#include <set>
#include <memory>
#include <mutex>
#include <filesystem>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QReadWriteLock>

#include <synthrt/Core/Core/Runtime.h>
#include <synthrt/Core/Support/Expected.h>
#include <synthrt/SVS/Inference.h>
#include <synthrt/SVS/InferenceContrib.h>

#include <synthrt/G2P/LanguageService.h>
#include <diffsinger/Infer/SingerStageResolver.h>
#include <diffsinger/Infer/ModelSet.h>
#include <diffsinger/Infer/InferenceService.h>
#include <synthrt/Driver/OnnxSetup.h>

#include "Model/AppModel/SingerIdentifier.h"
#include "PackageCatalog.h"

class SingerModelSession;
class AppContext;

class SynthrtEngine final : public QObject {
    Q_OBJECT

private:
    friend class AppContext;
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
    bool initialize(const QStringList &voicebankPaths, const QStringList &g2pPackagePaths,
                    const QString &ep = QStringLiteral("CPU"), int deviceIndex = 0);

    bool initialized() const;
    bool runtimeInitialized() const noexcept;
    bool pitchExtractionReady() const noexcept;
    bool midiExtractionReady() const noexcept;
    bool isAboutToQuit() const noexcept;
    void shutdown() noexcept;

    static std::filesystem::path pluginRoot();

    [[nodiscard]] RuntimeOperationLease acquirePitchExtractionOperation();
    [[nodiscard]] RuntimeOperationLease acquireMidiExtractionOperation();

    // === Voicebank scanner (replaces PackageManager scanning) ===
    //
    // Re-scan voicebank directories. Populates internal singer cache.
    // Returns package statuses (one per discovered package).
    srt::core::Expected<std::shared_ptr<const PackageCatalog::Snapshot>>
        refreshVoicebanks(const std::vector<std::filesystem::path> &searchPaths,
                          bool allowReuse = true);

    /// Cached singer snapshots from the last refreshVoicebanks().
    /// Lookup snapshot by lite SingerIdentifier.
    srt::core::Expected<ds::bank::SingerSnapshot>
        singerSnapshot(const SingerIdentifier &identifier) const;

    /// Lookup SingerIdentifier by singerId (scans all packages).
    srt::core::Expected<SingerIdentifier> findSinger(const QString &singerId) const;

    /// Exact package directory for a versioned singer identifier.
    std::filesystem::path packageDirectory(const SingerIdentifier &identifier) const;

    // === Singer model sessions ===
    //
    std::shared_ptr<SingerModelSession> acquireSingerSession(const SingerIdentifier &identifier);

    /// Release all cached singer sessions. Sessions retained by running tasks stay alive.
    void unloadSinger();

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

    // === Runtime access ===
    srt::core::Runtime &runtime();
    const srt::core::Runtime &runtime() const;

Q_SIGNALS:
    void singerLoaded(const SingerIdentifier &identifier);

private:
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_runtimeInitialized{false};
    std::atomic<bool> m_pitchExtractionReady{false};
    std::atomic<bool> m_midiExtractionReady{false};
    std::atomic<bool> m_aboutToQuit{false};

    // Components (v2 architecture: lite directly holds these)
    PackageCatalog m_catalog;
    srt::g2p::LanguageService m_langSvc;
    srt::core::Runtime m_runtime;

    std::unordered_map<SingerIdentifier, std::shared_ptr<SingerModelSession>> m_singerSessions;
    std::set<std::filesystem::path> m_loadedPackageDirs;

    std::mutex m_catalogRefreshMutex;
    mutable std::mutex m_stateMutex;
    mutable QReadWriteLock m_singerRwLock;
    mutable std::shared_mutex m_runtimeLifecycleMutex;

    // Internal helpers
    bool initializeRuntime(const std::filesystem::path &pluginRoot, const QString &ep,
                           int deviceIndex);
    void initializeExtractors(const std::filesystem::path &pluginRoot);
    bool initializeG2pOnnxDriver();
};

#endif // SYNTHRT_ENGINE_H
