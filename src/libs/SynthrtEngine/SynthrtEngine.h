#ifndef SYNTHRT_ENGINE_H
#define SYNTHRT_ENGINE_H

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <QObject>
#include <QString>
#include <QStringList>

#include <synthrt/Core/SynthUnit.h>
#include <synthrt/Support/Expected.h>

#include <lite/ProjectModel/AppModel/SingerIdentifier.h>

#include "LanguageBridge.h"
#include "VoicebankCatalog.h"

// Include AnalysisExecutive.h where an AnalyzerLease is destroyed.
namespace otter {
    class AnalysisExecutive;
}

namespace lite::synthrt {

    class SingerPipeline;
    class Bootstrap;

    QString f0Contract();

    QString noteContract();

}

class SynthrtEngine final : public QObject {
    Q_OBJECT

private:
    friend class SingletonRegistry; // constructed/destroyed via SingletonRegistry::create/destroy
    explicit SynthrtEngine(QObject *parent = nullptr);
    ~SynthrtEngine() override;

public:
    static SynthrtEngine &instance();

    Q_DISABLE_COPY_MOVE(SynthrtEngine)

    // Initialize with voicebank scan roots, dependency search paths, and deployment directories.
    // Voicebank roots are scanned one level deep; packagePaths is used for dependency resolution.
    bool initialize(const QStringList &voicebankPaths, const QStringList &packagePaths,
                    const QString &ep = QStringLiteral("CPU"), int deviceIndex = 0,
                    const std::filesystem::path &pluginRoot = defaultPluginRoot(),
                    const std::filesystem::path &runtimePath = defaultRuntimePath());

    bool initialized() const noexcept;

    /// Metadata and dictionary-only languages remain usable without an ONNX driver.
    bool inferenceReady() const;

    /// True once initialization was attempted, including a failed attempt.
    bool initializationDone() const noexcept;

    /// Releases waiters when initialization is rejected by the host or during shutdown.
    void completeInitializationAttempt() noexcept;

    static constexpr int kDefaultInitializationTimeoutMs = 30'000;

    /// Wait for an initialization attempt. Returns false on timeout; check initialized() for
    /// success.
    bool waitForInitialization(int timeoutMs = kDefaultInitializationTimeoutMs) const;

    bool isAboutToQuit() const noexcept;
    void shutdown() noexcept;

    static std::filesystem::path defaultPluginRoot();

    static std::filesystem::path defaultRuntimePath();

    /// CUDA payload directory below defaultRuntimePath().
    static std::filesystem::path defaultCudaRuntimePath();

    static std::filesystem::path defaultLanguagePackagePath();


    enum class RescanMode {
        /// Scan with a fresh unit. Existing leases retain the old unit until they finish.
        Reload,
        /// Reuse loaded handles when scanning, including the initial post-initialize scan.
        ReuseLoaded,
    };

    /// Publish a catalog and report packages that failed to open.
    /// Reload advances catalogGeneration(); ReuseLoaded advances it when package identities change.
    srt::Expected<std::vector<lite::synthrt::SingerEntry>>
        refreshVoicebanks(const std::vector<std::filesystem::path> &searchPaths,
                          std::vector<lite::synthrt::PackageProblem> *problems = nullptr,
                          RescanMode mode = RescanMode::Reload,
                          std::vector<lite::synthrt::PackageEntry> *packages = nullptr);

    /// Validate in an isolated unit without changing the live catalog.
    srt::Expected<void> validatePackage(const std::filesystem::path &path) const;

    std::vector<lite::synthrt::SingerEntry> singers() const;

    /// Resolve an exact package version, or the highest loaded version when unspecified.
    srt::Expected<lite::synthrt::SingerEntry> singer(const SingerIdentifier &identifier) const;


    /// Lazily create a shared pipeline. Active holders retain its package across refreshes.
    /// The engine holds only a weak reference; the last holder releases the models.
    srt::Expected<std::shared_ptr<lite::synthrt::SingerPipeline>>
        pipelineFor(const SingerIdentifier &identifier);

    /// Catalog revision used to invalidate cached model data.
    std::uint64_t catalogGeneration() const noexcept;


    /// List the highest loaded version of each analyzer, sorted by package and contribution.
    /// An empty interfaceName selects all supported contracts.
    std::vector<lite::synthrt::AnalyzerEntry>
        analyzers(const QString &interfaceName = QString()) const;

    /// Own the runtime, package, and executive in destruction-safe order.
    struct AnalyzerLease {
        /// Declared first so the executive and package are destroyed before their runtime.
        std::shared_ptr<lite::synthrt::Bootstrap> unitOwner;
        srt::PackageHandle package;
        /// Borrowed declaration, valid while the package is held.
        const srt::ContribSpec *spec = nullptr;
        std::unique_ptr<otter::AnalysisExecutive> executive;
    };

    /// Create an uncached analyzer using the highest loaded package version.
    srt::Expected<AnalyzerLease> createAnalyzer(const QString &reference);


    /// Query language availability without loading models or packages.
    bool canConvert(const SingerIdentifier &identifier, const QString &language) const;

    /// Failure reason, or an empty string for an available route.
    QString languageUnavailableReason(const SingerIdentifier &identifier,
                                      const QString &language) const;

    /// Deepest usable conversion layer, or no value. See LanguageBridge::maxDepth().
    std::optional<lite::synthrt::LanguageBridge::Depth> maxDepth(const SingerIdentifier &identifier,
                                                                 const QString &language) const;

    srt::Expected<std::vector<lite::synthrt::LanguageBridge::Result>>
        convert(const SingerIdentifier &identifier, const QString &language,
                const std::vector<lite::synthrt::LanguageBridge::Word> &words,
                lite::synthrt::LanguageBridge::Depth depth =
                    lite::synthrt::LanguageBridge::Depth::Onsets) const;

    /// Forced SP/AP markers plus the singer's declared markers, without duplicates.
    std::vector<std::string> reservedPhonemesOf(const SingerIdentifier &identifier) const;


    /// Borrowed unit pointer, or null before initialization and after shutdown.
    /// Valid only until the next reload or shutdown; prefer the scoped operations above.
    srt::SynthUnit *unitIfReady() noexcept;

signals:
    /// Synchronous cache invalidation before a reload releases and reopens package handles.
    /// Emitted before acquiring the SDK lifecycle lock so model teardown can serialize safely.
    void voicebanksAboutToReload();

    /// Emitted for route/executive failures, not per-word failures.
    /// Consumers must refresh cached language availability.
    void languageRouteFailed(const QString &language);

private:
    /// Look up a declaration and optionally its package handle. Requires the lifecycle lock.
    srt::ContribSpec *findAnalyzer(const QString &reference,
                                   const srt::PackageHandle **package = nullptr) const;

    class Impl;
    std::unique_ptr<Impl> _impl;
};

#endif // SYNTHRT_ENGINE_H
