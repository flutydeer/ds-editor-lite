//
// SynthrtEngine - the editor's single entry point to synthrt, wolf and otter.
//
// The engine delegates to one synthesis unit and to the components that read from it. It replaces
// the refactor line's facade over a Runtime, a LanguageService, a VoicebankSession and a plugin
// factory, which required a two-stage initialization and a deferred model load.
//
// The components are in this directory and are tested separately:
//
//   SynthrtBootstrap   the unit, the category plugin paths, the ONNX driver
//   VoicebankCatalog   package scanning and derivation of singer capabilities
//   SingerPipeline     the five inference stages of a singer
//   LanguageBridge     grapheme-to-phoneme conversion through wolf
//
// Every method is thread-safe: the engine state is guarded by one lifecycle lock or is atomic. No
// method terminates the process if the unit is absent: the methods return a failure or an empty
// result instead, and unitIfReady() returns nullptr.
//
// instance() is the only fatal precondition, and its declaration states the consequence: it
// terminates the process with qFatal in every build configuration, before an owner has registered
// the engine.
//

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

// Declared rather than included, so that a file including the engine does not compile the otter
// contract headers and the pipeline headers of dsinfer. A file that destroys an AnalyzerLease
// includes <otter/Analysis/AnalysisExecutive.h>, and a file that uses a pipeline includes
// SingerPipeline.h.
namespace otter {
    class AnalysisExecutive;
}

namespace lite::synthrt {

    class SingerPipeline;

    /// Returns the interface name of otter's F0 analysis contract.
    ///
    /// A host requests analyzers one contract at a time, and otter owns the contract identifiers.
    /// Both contract names are read from otter's headers, so that a rename in otter causes a
    /// compile error rather than a filter that silently matches nothing.
    QString f0Contract();

    /// Returns the interface name of otter's Note analysis contract.
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

    // === Initialization (call once at startup) ===
    //
    // Initialization has a single stage, with no advance warm-up: wolf loads a language on its
    // first conversion.
    //
    // voicebankPaths — directories containing packages, searched one level deep
    // packagePaths   — dependency search paths. This list differs from voicebankPaths: a
    //                  voicebank scan opens every package found, whereas a dependency is resolved
    //                  through these paths
    // ep             — execution provider name, as spelled by lite::synthrt::backendName()
    // pluginRoot / runtimePath — deployment directories of the plugin trees and ONNX Runtime.
    //                  They default to the locations of this build relative to the executable.
    //                  They are parameters so that a layout other than the installed editor, such
    //                  as a test or a portable installation, can specify the actual locations.
    bool initialize(const QStringList &voicebankPaths, const QStringList &packagePaths,
                    const QString &ep = QStringLiteral("CPU"), int deviceIndex = 0,
                    const std::filesystem::path &pluginRoot = defaultPluginRoot(),
                    const std::filesystem::path &runtimePath = defaultRuntimePath());

    bool initialized() const noexcept;

    /// Returns whether initialize() has been attempted, regardless of its result. A caller that
    /// requires the engine can poll this to detect a failure instead of waiting indefinitely.
    bool initializationDone() const noexcept;

    /// Default timeout of waitForInitialization(). Initialization loads plugins and scans the
    /// voicebanks, which takes seconds on a slow disk. A caller that exceeds this timeout reports
    /// a timeout instead of blocking indefinitely.
    static constexpr int kDefaultInitializationTimeoutMs = 30'000;

    /// Blocks until initialize() has been attempted or \a timeoutMs elapses.
    ///
    /// \return true if initialize() was attempted within the timeout, false on timeout.
    ///         initialized() reports whether the initialization succeeded.
    bool waitForInitialization(int timeoutMs = kDefaultInitializationTimeoutMs) const;

    bool isAboutToQuit() const noexcept;
    void shutdown() noexcept;

    /// Returns the plugin directory of this build, derived from the executable location.
    static std::filesystem::path defaultPluginRoot();

    /// Returns the ONNX Runtime directory of this build. The application specifies this path
    /// explicitly instead of searching for a runtime.
    static std::filesystem::path defaultRuntimePath();

    /// Returns the directory of the CUDA flavor of ONNX Runtime, a subdirectory of
    /// defaultRuntimePath().
    static std::filesystem::path defaultCudaRuntimePath();

    /// Returns the directory of the wolf language packages through which voicebank dependencies
    /// resolve.
    static std::filesystem::path defaultLanguagePackagePath();

    // === Voicebanks ===

    /// How a rescan treats the packages that are already loaded.
    enum class RescanMode {
        /// Releases the handles of the engine and the language session before the scan, so that
        /// a package changed on disk is read again. A package that a running synthesis still
        /// holds stays loaded and is taken over as it is.
        Reload,
        /// Keeps the handles during the scan, so that a package that is already loaded is taken
        /// over without being read again. Intended for the first scan after initialize(), which
        /// scanned the same paths shortly before.
        ReuseLoaded,
    };

    /// Rescans the search paths and publishes the catalog.
    ///
    /// catalogGeneration() changes only if the set of packages, by identifier, version and
    /// directory, differs from the set before the scan.
    ///
    /// A package that fails to open is reported rather than treated as fatal; scan() in
    /// VoicebankCatalog states that policy and records the reason. A caller may also pass
    /// \a problems to display the reasons to the user, as the package list does, because an
    /// installed voicebank that is not listed requires an explanation.
    srt::Expected<std::vector<lite::synthrt::SingerEntry>>
        refreshVoicebanks(const std::vector<std::filesystem::path> &searchPaths,
                          std::vector<lite::synthrt::PackageProblem> *problems = nullptr,
                          RescanMode mode = RescanMode::Reload);

    /// Returns the singers found by the last scan.
    std::vector<lite::synthrt::SingerEntry> singers() const;

    /// Returns the singer with \a identifier, or an error if the singer is not loaded.
    ///
    /// An identifier with a version matches that version only. An identifier without one
    /// resolves to the highest loaded version of the package; pipelineFor() and the language calls
    /// resolve it the same way.
    srt::Expected<lite::synthrt::SingerEntry> singer(const SingerIdentifier &identifier) const;

    // === Inference ===

    /// Returns the pipeline for a singer, shared among all current holders.
    ///
    /// The pipeline is built on first use. While any holder retains it, a later request returns
    /// the same pipeline, so its five models are opened once. The pipeline keeps its package
    /// loaded, so a holder may retain it across a refresh and complete its work; releasing the
    /// last reference closes the models. The engine keeps no reference of its own, so the
    /// retention policy belongs to the caller.
    srt::Expected<std::shared_ptr<lite::synthrt::SingerPipeline>>
        pipelineFor(const SingerIdentifier &identifier);

    /// Returns the number of catalog republications, that is, rescans that changed the set of
    /// packages.
    ///
    /// A pipeline obtained before such a refresh remains usable but describes the voicebank as it
    /// was scanned at that time. A cache that requires the current pipeline records this value
    /// when it obtains a pipeline and compares the value before reuse.
    std::uint64_t catalogGeneration() const noexcept;

    // === Analysis ===

    /// Returns the analyzers found by the last scan, optionally restricted to one contract.
    ///
    /// A reference carries no version, so each reference is listed once, with the highest loaded
    /// version of its package, which is the version that createAnalyzer() runs. The list is sorted
    /// by package and contribution.
    ///
    /// \a interfaceName is otter's contract identifier: F0 for a pitch curve, Note for a
    /// transcription. An empty \a interfaceName selects every contract. The filter is applied here
    /// because a settings page lists one contract at a time.
    std::vector<lite::synthrt::AnalyzerEntry>
        analyzers(const QString &interfaceName = QString()) const;

    /// An analyzer together with a handle of the package that contains its declaration.
    ///
    /// The executive reads its declaration, which belongs to the package, and synthrt terminates
    /// the process if a package is released while any of its executives exists. The handle keeps
    /// the package loaded for the lifetime of the lease, so that a refresh during an extraction
    /// cannot release it, and the member order ensures that the executive is destroyed before the
    /// handle is released.
    struct AnalyzerLease {
        srt::PackageHandle package;
        /// The declaration behind the reference, used to read the input format of the analyzer.
        /// Valid while \c package is held.
        const srt::ContribSpec *spec = nullptr;
        std::unique_ptr<otter::AnalysisExecutive> executive;
    };

    /// Builds an analyzer from the reference that the editor stored, or returns an error that
    /// describes the failure.
    ///
    /// If several versions of the package are loaded, the highest version is used.
    ///
    /// Unlike a pipeline, the analyzer is not cached: an extraction is a single operation, and
    /// keeping a model open between two extractions wastes memory.
    srt::Expected<AnalyzerLease> createAnalyzer(const QString &reference);

    // === Language ===

    /// Returns whether a singer can convert a language. The query loads no model or package.
    bool canConvert(const SingerIdentifier &identifier, const QString &language) const;

    /// Returns the reason a singer cannot convert \a language, or an empty string when it can.
    /// The query loads nothing. A language the singer does not declare also reports a reason.
    QString languageUnavailableReason(const SingerIdentifier &identifier,
                                      const QString &language) const;

    /// Returns the deepest layer that a singer reaches for \a language, or an empty value when the
    /// route is not usable. The query loads nothing; LanguageBridge::maxDepth documents why the
    /// value is read before a conversion instead of being inferred from its output.
    std::optional<lite::synthrt::LanguageBridge::Depth> maxDepth(const SingerIdentifier &identifier,
                                                                 const QString &language) const;

    /// Converts one batch of words in a single language. LanguageBridge describes the fields of a
    /// word.
    srt::Expected<std::vector<lite::synthrt::LanguageBridge::Result>>
        convert(const SingerIdentifier &identifier, const QString &language,
                const std::vector<lite::synthrt::LanguageBridge::Word> &words,
                lite::synthrt::LanguageBridge::Depth depth =
                    lite::synthrt::LanguageBridge::Depth::Onsets) const;

    /// Returns the reserved phonemes of a singer: FORCED_RESERVED_PHONEMES, followed by those that
    /// the singer declares, or only FORCED_RESERVED_PHONEMES if the singer is not loaded. Every
    /// inference task tests a lyric or a phoneme against this set; see ReservedPhonemes.h.
    std::vector<std::string> reservedPhonemesOf(const SingerIdentifier &identifier) const;

    // === Unit access ===
    //
    // Direct access is intended for the few operations that require it, such as opening a package
    // supplied to the editor directly. The functions above are preferred.
    //
    // The function does not wait for the engine; unitIfReady() states the interval in which the
    // unit exists.

    /// Returns the unit, or nullptr before a successful initialize() and after shutdown(). The
    /// pointer remains valid until shutdown() and dangles if shutdown() runs while the caller holds
    /// it.
    srt::SynthUnit *unitIfReady() noexcept;

signals:
    /// Emitted after a conversion failed at the route or executive layer, carrying the language
    /// that failed.
    ///
    /// wolf records such a failure in its per (singer, language) failure cache, so a later probe()
    /// reports Unavailable and repeats that reason the way wolf renders it -- the kind of the error
    /// code followed by the whole cause chain, not the line of the failing layer alone. Only wolf's
    /// refresh() clears that cache. A per-word failure, which is the error field of a result, is
    /// not cached and therefore does not emit this signal. A caller that has cached language
    /// availability must query again after this signal, or its presentation stays at the state
    /// from before the failure.
    void languageRouteFailed(const QString &language);

private:
    /// Returns the declaration of the analyzer identified by \a reference, or null if no loaded
    /// package contains it. If \a package is not null, stores the handle of the containing
    /// package in \a package. The caller must hold the lifecycle lock.
    srt::ContribSpec *findAnalyzer(const QString &reference,
                                   const srt::PackageHandle **package = nullptr) const;

    class Impl;
    std::unique_ptr<Impl> _impl;
};

#endif // SYNTHRT_ENGINE_H
