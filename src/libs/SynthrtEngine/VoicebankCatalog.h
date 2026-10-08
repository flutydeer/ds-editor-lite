#ifndef VOICEBANKCATALOG_H
#define VOICEBANKCATALOG_H

#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <stdcorelib/support/versionnumber.h>

#include <synthrt/Core/PackageHandle.h>
#include <synthrt/Core/SynthUnit.h>
#include <synthrt/SVS/InferenceContrib.h>
#include <synthrt/SVS/SingerContrib.h>
#include <synthrt/Support/DisplayText.h>
#include <synthrt/Support/Expected.h>

#include "AnalyzerReference.h"

/// Voicebank catalog: package scanning and derivation of singer capabilities.
///
/// synthrt defines no scanner, snapshot or capability report type, unlike the VoicebankScanner and
/// SingerCapabilityReport of the refactor line. It provides packages, singer declarations and the
/// imports between them, and a host derives the capabilities of a singer from these. The
/// derivation is separate from the engine so that it can be tested against a real package without
/// building the rest of the editor.
namespace wolf {
    class LinguistSession;
}

namespace lite::synthrt {

    namespace fs = std::filesystem;

    class LanguageBridge;

    /// A speaker that a singer offers.
    ///
    /// \c id is the host identifier of the speaker: the identifier from the singer declaration,
    /// which is also the key of the speaker mapping of the acoustic import. Hosts store, display
    /// and address speakers by host identifier. The speaker name of the acoustic model is the
    /// target of the mapping and is internal to the model. The display name and the tone range
    /// come from the singer declaration, which carries host-only data; no interpreter reads either
    /// field.
    struct SpeakerCapability {
        std::string id;

        /// Display name. Defaults to the identifier if the singer declares no name.
        srt::DisplayText name;

        /// Lowest and highest MIDI note intended for this speaker, if the singer declares a range.
        std::optional<std::pair<int, int>> toneRange;
    };

    /// Capabilities of a singer, derived from its imports and their targets.
    ///
    /// A singer declares no capabilities directly. For example, a singer imports a variance model,
    /// and the exports of that model specify which parameters it predicts. The derivation prevents
    /// a voicebank from claiming a capability that its models lack.
    struct SingerCapabilities {
        /// Stages that the singer imports. The acoustic and vocoder stages are required of every
        /// singer. The other three are optional, and a singer that lacks an optional stage cannot
        /// perform it.
        bool duration = false;
        bool pitch = false;
        bool variance = false;
        bool acoustic = false;
        bool vocoder = false;

        /// Speakers that this singer offers, identified by host identifier.
        std::vector<SpeakerCapability> speakers;

        /// Host identifiers that no synthesis can reach, in reading order.
        ///
        /// Only one direction is checked. An entry is a host identifier from the singer
        /// declaration, or from the import mapping if the singer declares no speakers, that the
        /// inference tasks cannot translate into a speaker exported by the acoustic model. A model
        /// speaker that no declaration lists is not reported: without a mapping, a synthesis
        /// accepts the model's speaker name unchanged, and with a mapping, no host identifier
        /// refers to that speaker. Every identifier listed here is a packaging fault and is absent
        /// from \c speakers, because a synthesis addressed with such an identifier would fail late,
        /// in a task that has already read the project and started the models. The identifiers are
        /// recorded rather than discarded so that the fault is reported instead of hidden.
        std::vector<std::string> unaddressableSpeakers;

        /// Variance parameters the models between them can predict.
        std::set<std::string> variancePredictions;

        /// Variance parameters the acoustic model consumes.
        std::set<std::string> varianceControls;

        /// Transition parameters the acoustic model accepts.
        std::set<std::string> transitionControls;

        /// Whether the pitch model accepts an expressiveness curve.
        bool allowsExpressiveness = false;

        /// The frame width, in seconds, of each frame-based model, as specified by its
        /// configuration: the frame width of the duration, pitch and variance models, and the hop
        /// size divided by the sample rate for the acoustic model. Zero if the singer lacks the
        /// stage.
        double durationFrameWidth = 0;
        double pitchFrameWidth = 0;
        double varianceFrameWidth = 0;
        double acousticFrameWidth = 0;

        /// The sample rate, in hertz, of the audio that the vocoder produces. Zero if the singer
        /// has no vocoder.
        int vocoderSampleRate = 0;

        /// Language handles the singer declares, and which of them is its default.
        std::vector<std::string> languages;
        std::string defaultLanguage;

        /// Phoneme tokens that a lyric may specify directly and that no language produces.
        ///
        /// Examples are a breath, a glottal stop and a hum. The author writes the token itself in
        /// place of a lyric, and the token reaches the models unchanged. The singer declares these
        /// tokens, and the loader has already verified that the singer's models support each of
        /// them; a singer with an invalid list fails to load, so no check is required here. The
        /// language layer receives these tokens and converts such a word directly instead of
        /// through a language.
        std::vector<std::string> reservedPhonemes;

        /// Phonemes that every frame model of the singer can produce, sorted and duplicate free.
        ///
        /// Derived from the phoneme table that each of the duration, pitch, variance and acoustic
        /// models declares in its configuration. A table is keyed `<language>/<phoneme>`, and this
        /// list holds the keys with that prefix removed, because the prefix is not part of a
        /// phoneme and a host that supplies a keyed token measures a coverage of zero. A key
        /// without a prefix is language independent and is kept unchanged.
        ///
        /// The list is the intersection of the tables, because a symbol that only some of the
        /// models know is worse than a symbol that none of them knows: a language that uses it
        /// would sound for part of the phrase only. A model that declares no table is not
        /// intersected, so that a singer with one empty table is not reported as singing nothing.
        ///
        /// An empty list means that no model declared a table, which is not the same as a singer
        /// with no phoneme, and a host must not report it as an empty coverage for that reason.
        std::vector<std::string> singablePhonemes;

        /// Returns whether every stage that a synthesis requires is present.
        bool complete() const {
            return acoustic && vocoder;
        }
    };

    /// A singer contributed by a loaded package.
    ///
    /// The package fields are repeated on every singer of a package rather than stored once beside
    /// them. Hosts name, hold and display singers, and recording the originating package on each
    /// entry costs a few strings and saves every caller a second lookup.
    struct SingerEntry {
        std::string packageId;
        stdc::VersionNumber packageVersion;
        std::string contributionId;
        srt::DisplayText name;
        fs::path packagePath;
        SingerCapabilities capabilities;

        /// Package metadata, for a host that lists packages rather than singers. Each field is
        /// empty if the package does not declare it.
        srt::DisplayText packageName;
        srt::DisplayText packageVendor;
        srt::DisplayText packageDescription;
        srt::DisplayText packageCopyright;
        std::string packageUrl;
    };

    /// Returns whether \a interfaceName names an analysis contract that this build runs.
    ///
    /// Analysers are inference modules and therefore share their category with every model that a
    /// voicebank ships; the contract distinguishes them. The accepted contracts are the entries
    /// of findAnalysisContract() in AnalysisContracts.h, which also creates their analyzers.
    bool isAnalysisContract(std::string_view interfaceName);

    /// An analyzer contributed by a loaded package.
    ///
    /// The structure mirrors SingerEntry because an analyzer is discovered the same way: a package
    /// contributes it, and its declaration specifies its capabilities. The interface identifies the
    /// implemented contract, either a pitch curve or a note transcription, so a host selects
    /// analyzers by interface.
    struct AnalyzerEntry {
        std::string packageId;
        stdc::VersionNumber packageVersion;
        std::string contributionId;
        std::string interfaceName;
        std::string variant;
        srt::DisplayText name;

        /// The languages that a note analyzer distinguishes and the default language used if an
        /// execution specifies none, as declared by its exports. Both are empty for an analyzer
        /// that distinguishes no languages, which includes every pitch analyzer.
        std::vector<std::string> languages;
        std::string defaultLanguage;

        /// Returns the language that an execution requesting \a requested uses.
        ///
        /// An analyzer that distinguishes no languages accepts any request, so \a requested is
        /// returned. A listed language is returned unchanged. An empty or unlisted request is
        /// replaced by the declared default, because the analyzer rejects an unlisted language.
        std::string effectiveLanguage(const std::string &requested) const {
            if (languages.empty()) {
                return requested;
            }
            for (const auto &language : languages) {
                if (language == requested) {
                    return requested;
                }
            }
            return defaultLanguage;
        }

        /// Returns the reference that the editor stores if a user selects this analyzer. Unlike a
        /// path, which changes if the package is reinstalled elsewhere, the reference is stable.
        std::string reference() const {
            return AnalyzerReference{packageId, contributionId}.toString();
        }
    };

    /// A package that failed to open, and the reason for the failure.
    struct PackageProblem {
        fs::path path;
        std::string reason;
    };

    /// Derives the capabilities of \a singer.
    ///
    /// Reads only the declaration and the exports of the import targets, so it loads no model and
    /// can be called while voicebanks are listed.
    SingerCapabilities capabilitiesOf(const srt::SingerSpec &singer);

    /// Supplies the phoneme tables that capabilitiesOf() derived to the language layer, so that it
    /// reports the coverage of a language instead of an unknown coverage.
    ///
    /// wolf does not read voicebank formats and takes the table from the host, which is this
    /// function. A singer whose derived table is empty is skipped rather than supplied with an
    /// empty list, because an empty list would declare that the singer covers no phoneme and every
    /// language with a complete inventory would then be reported as unsingable. A singer that
    /// declares no language is skipped as well, because the language layer holds no entry for it
    /// and would warn.
    ///
    /// Call this directly after every rebuild of the language catalog. A table survives a rebuild
    /// only for a singer that the new catalog still holds, and a singer that is still held keeps
    /// the table it was given, so a repeated call is without effect and no state is cached here.
    ///
    /// \a session and \a bridge are the two ways to reach that layer: a host that inspects wolf
    /// directly holds the session, and the editor holds the bridge that wraps one.
    /// \{
    void feedSingerPhonemes(wolf::LinguistSession &session, const std::vector<SingerEntry> &catalog);
    void feedSingerPhonemes(LanguageBridge &language, const std::vector<SingerEntry> &catalog);
    /// \}

    /// Opens every package directly under each of \a paths and returns the singers they contain.
    ///
    /// This function is the scanner that synthrt does not provide; see the file header. The search
    /// is one directory level deep, and a package is a directory that contains a desc.json. A
    /// package that fails to open is reported in \a problems and skipped rather than treated as
    /// fatal, so that one broken voicebank does not hide the others.
    ///
    /// The handles appended to \a opened keep their packages loaded. The caller owns them and must
    /// release them before the unit is destroyed.
    std::vector<SingerEntry> scan(srt::SynthUnit &unit, const std::vector<fs::path> &paths,
                                  std::vector<srt::PackageHandle> &opened,
                                  std::vector<PackageProblem> &problems);

    /// Returns the analyzers among the already opened packages, that is, their inference modules
    /// whose contract isAnalysisContract() accepts.
    ///
    /// Separate from scan() because analyzers ship in their own packages and are not voicebank
    /// content. One directory walk opens both kinds of package, and this function reads the
    /// analyzers from the opened packages.
    std::vector<AnalyzerEntry> analyzersOf(const std::vector<srt::PackageHandle> &opened);

}

#endif // VOICEBANKCATALOG_H
