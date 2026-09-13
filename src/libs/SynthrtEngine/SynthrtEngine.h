//
// SynthrtEngine - the editor's one way into synthrt, wolf and otter.
//
// On the refactor line this was a facade over four things the editor had to keep in step: a
// Runtime, a LanguageService, a VoicebankSession and a plugin factory, with a two stage
// initialisation and a deferred model load arranged around them. On the main line those have
// become one unit and a few parts that read from it, so this is mostly delegation now.
//
// The parts live beside this file and are tested on their own:
//
//   SynthrtBootstrap   the unit, the category plugin paths, the ONNX driver
//   VoicebankCatalog   scanning packages and working out what each singer can do
//   SingerPipeline     a singer's five inference stages
//   LanguageBridge     grapheme to phoneme, through wolf
//
// Everything here is safe to call from any thread.
//

#ifndef SYNTHRT_ENGINE_H
#define SYNTHRT_ENGINE_H

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <QObject>
#include <QString>
#include <QStringList>

#include <synthrt/Core/SynthUnit.h>
#include <synthrt/Support/Expected.h>

#include <lite/ProjectModel/AppModel/SingerIdentifier.h>

#include <otter/Analysis/AnalysisExecutive.h>
#include <otter/Api/F0/1/F0ApiL1.h>
#include <otter/Api/Note/1/NoteApiL1.h>

#include "LanguageBridge.h"
#include "SingerPipeline.h"
#include "SynthrtBootstrap.h"
#include "VoicebankCatalog.h"

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
    // One stage now. The refactor line split this in two because loading every language's models
    // took long enough to need deferring; wolf loads a language when a conversion first asks for
    // one, so there is no second stage to wait for and nothing to warm up in advance.
    //
    // voicebankPaths — directories holding packages, searched one level deep
    // packagePaths   — where a dependency is looked for, which is not the same list: a voicebank
    //                  scan opens what it finds, a dependency is resolved through these
    // ep             — execution provider name ("CPU" / "DirectML" / "CUDA" / "CoreML")
    // pluginRoot / runtimePath — where the plugin trees and ONNX Runtime were deployed. They
    //                  default to where this build puts them, relative to the executable, and are
    //                  parameters so that something other than the installed editor -- a test, a
    //                  portable layout -- can say where they really are instead.
    bool initialize(const QStringList &voicebankPaths, const QStringList &packagePaths,
                    const QString &ep = QStringLiteral("CPU"), int deviceIndex = 0,
                    const std::filesystem::path &pluginRoot = defaultPluginRoot(),
                    const std::filesystem::path &runtimePath = defaultRuntimePath());

    bool initialized() const noexcept;

    /// True after initialize() has been attempted, whether or not it worked. A caller that needs
    /// the engine can poll this to notice a failure instead of waiting forever.
    bool initializationDone() const noexcept;

    /// Blocks until initialize() has been attempted, or \a timeoutMs elapses. Returns false on
    /// timeout; check initialized() afterwards to see whether it succeeded.
    bool waitForInitialization(int timeoutMs = 30000) const;

    /// Whether a model can be opened at all. False when no ONNX driver plugin was found, in which
    /// case voicebanks still list and only inference is unavailable.
    bool hasInferenceBackend() const noexcept;

    bool isAboutToQuit() const noexcept;
    void shutdown() noexcept;

    /// Where this build puts the plugin trees, relative to the executable.
    static std::filesystem::path defaultPluginRoot();

    /// Where this build puts ONNX Runtime. Named by the application rather than searched for.
    static std::filesystem::path defaultRuntimePath();

    // === Voicebanks ===

    /// Rescans the search paths and republishes the catalogue.
    ///
    /// A package that will not open is reported rather than fatal, so one broken voicebank cannot
    /// hide the rest. The reasons are always logged; pass \a problems as well to show them to a
    /// person, which is what the package list does -- a voicebank someone installed and cannot
    /// see needs to say why, not merely be absent.
    srt::Expected<std::vector<lite::synthrt::SingerEntry>>
        refreshVoicebanks(const std::vector<std::filesystem::path> &searchPaths,
                          std::vector<lite::synthrt::PackageProblem> *problems = nullptr);

    /// The singers the last scan found.
    std::vector<lite::synthrt::SingerEntry> singers() const;

    /// One singer by identifier, or an error when it is not loaded.
    srt::Expected<lite::synthrt::SingerEntry> singer(const SingerIdentifier &identifier) const;

    /// Finds a singer by its contribution id alone, across every loaded package.
    srt::Expected<SingerIdentifier> findSinger(const QString &singerId) const;

    /// Where a singer's package sits on disk.
    std::filesystem::path packageDirectory(const SingerIdentifier &identifier) const;

    // === Inference ===

    /// The pipeline for a singer, shared with everyone who currently holds it.
    ///
    /// Built on first use; while any holder keeps it, a second request returns the same one, so
    /// the five models it opened are opened once. The pipeline keeps its package loaded, so a
    /// holder may keep it across a refresh and finish what it was doing; letting the last
    /// reference go is what closes the models. The engine keeps no reference of its own: what to
    /// retain, and for how long, is the caller's policy.
    srt::Expected<std::shared_ptr<lite::synthrt::SingerPipeline>>
        pipelineFor(const SingerIdentifier &identifier);

    /// Counts republications of the catalogue.
    ///
    /// A pipeline taken before a refresh stays usable, but describes the voicebank as it was
    /// scanned then. A cache that wants the current one reads this when it takes a pipeline and
    /// compares before reusing it.
    std::uint64_t catalogGeneration() const noexcept;

    // === Analysis ===

    /// The analysers the last scan found, optionally only those answering one contract.
    ///
    /// \a interfaceName is otter's contract identifier: F0 for a pitch curve, Note for a
    /// transcription. A settings page lists one kind at a time, which is why it filters here
    /// rather than afterwards.
    std::vector<lite::synthrt::AnalyzerEntry>
        analyzers(const QString &interfaceName = QString()) const;

    /// An analyser together with the package it borrows from.
    ///
    /// The executive reads its declaration, and the declaration belongs to the package. A refresh
    /// that released the package while an extraction ran would leave the executive over freed
    /// memory, and synthrt refuses to release a package whose executives are still alive. The
    /// handle keeps the package loaded for as long as the lease lives, and the members are
    /// declared so that the executive is destroyed before the handle lets go.
    struct AnalyzerLease {
        srt::PackageHandle package;
        /// The declaration behind the reference, for reading what format the analyser needs.
        /// Valid for as long as \c package is held.
        const srt::ContribSpec *spec = nullptr;
        std::unique_ptr<otter::AnalysisExecutive> executive;
    };

    /// Builds an analyser from the reference the editor stored, or says why it could not.
    ///
    /// Unlike a pipeline it is not cached: an extraction is a one-off, and holding a model open
    /// between two of them costs memory for nothing.
    srt::Expected<AnalyzerLease> createAnalyzer(const QString &reference);

    // === Language ===

    /// The languages a singer declares.
    QStringList languagesOf(const SingerIdentifier &identifier) const;

    /// Whether a singer can convert a language, asked without loading anything.
    bool canConvert(const SingerIdentifier &identifier, const QString &language) const;

    /// Converts one batch, all in one language. See LanguageBridge for what a word carries.
    srt::Expected<std::vector<lite::synthrt::LanguageBridge::Result>>
        convert(const SingerIdentifier &identifier, const QString &language,
                const std::vector<lite::synthrt::LanguageBridge::Word> &words,
                lite::synthrt::LanguageBridge::Depth depth =
                    lite::synthrt::LanguageBridge::Depth::Onsets) const;

    /// Asks any conversion in flight to stop.
    void cancelConversions();

    /// Tells the language layer which phonemes a singer can sing, so coverage can be reported.
    void setSingerPhonemes(const SingerIdentifier &identifier, std::vector<std::string> phonemes);

    /// The markers a lyric may be, answered without going through grapheme-to-phoneme.
    ///
    /// Defaults to the ecosystem's SP and AP. A voicebank is allowed to bring more of its own,
    /// and reading which ones is the editor's job -- the language layer does not open voicebank
    /// formats, the same reason setSingerPhonemes() exists. See LanguageBridge for what a marker
    /// converts to.
    void setReservedMarkers(std::vector<std::string> markers);
    std::vector<std::string> reservedMarkers() const;

    // === The unit itself ===
    //
    // For the few things that legitimately need it, such as opening a package the editor was
    // handed directly. Prefer the functions above.
    srt::SynthUnit &unit();

private:
    /// Finds an analyser's declaration and, when asked, the handle of the package holding it.
    /// For callers that already hold the lifecycle lock.
    srt::ContribSpec *findAnalyzer(const QString &reference,
                                   const srt::PackageHandle **package = nullptr) const;

    class Impl;
    std::unique_ptr<Impl> _impl;
};

#endif // SYNTHRT_ENGINE_H
