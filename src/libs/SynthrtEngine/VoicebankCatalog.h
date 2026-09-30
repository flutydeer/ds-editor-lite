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

/// Reading what a voicebank offers, on the main line.
///
/// The refactor line answered these questions with types of its own -- a VoicebankScanner, a
/// snapshot, a SingerCapabilityReport. The main line has none of them: it has packages, singer
/// declarations, and the imports between them, and what a singer can do is something a host works
/// out from those. This is that work, kept apart from the engine so it can be tested against a
/// real package without the rest of the editor having to build.
namespace lite::synthrt {

    namespace fs = std::filesystem;

    /// One speaker a singer offers.
    ///
    /// The identifier is the singer's own -- the one its declaration gives, and the one its import
    /// mapping is keyed by -- because that is the space a host stores, shows and addresses a
    /// speaker in. The acoustic model's name for the same speaker is what the mapping translates
    /// to, and is not what anyone outside the model should hold. The display name and the tone
    /// range come from the singer declaration, which is where a host puts what only a host reads:
    /// no interpreter on this line looks at either.
    struct SpeakerCapability {
        std::string id;

        /// What to show a person. Falls back to the identifier when the singer names nothing.
        srt::DisplayText name;

        /// Lowest and highest MIDI note this speaker is meant for, when the singer says.
        std::optional<std::pair<int, int>> toneRange;
    };

    /// What one singer can do, derived from its imports and what they reach.
    ///
    /// Derived rather than declared: nothing in a singer says "I can predict tension". It imports
    /// a variance model, and that model's exports say which parameters it predicts. Asking the
    /// question this way is why a voicebank cannot claim a capability its models do not have.
    struct SingerCapabilities {
        /// Which stages the singer imports. Acoustic and vocoder are required of every singer;
        /// the other three are optional and a singer without them cannot do that stage.
        bool duration = false;
        bool pitch = false;
        bool variance = false;
        bool acoustic = false;
        bool vocoder = false;

        /// The speakers this singer offers, in the identifiers a host names them by.
        std::vector<SpeakerCapability> speakers;

        /// Speaker identifiers a host could name that a synthesis cannot reach, in the order they
        /// were read.
        ///
        /// One direction only: an identifier the singer gives -- its declaration's, or, for a
        /// singer that declares no speakers, its import mapping's -- that the inference tasks
        /// cannot turn into a speaker the acoustic model exports. A speaker the model has that
        /// nothing declares is not reported: with no mapping its own name is what a synthesis
        /// accepts unchanged, and with a mapping no host has a name for it to have used. Every
        /// identifier listed here is a packaging fault, and none of them appears in \c speakers:
        /// a synthesis addressed with one would fail at the last moment, in a task that has
        /// already read the project and started the models. Recorded rather than dropped so the
        /// fault can be said out loud instead of hidden.
        std::vector<std::string> unaddressableSpeakers;

        /// Variance parameters the models between them can predict.
        std::set<std::string> variancePredictions;

        /// Variance parameters the acoustic model consumes.
        std::set<std::string> varianceControls;

        /// Transition parameters the acoustic model accepts.
        std::set<std::string> transitionControls;

        /// Whether the pitch model accepts an expressiveness curve.
        bool allowsExpressiveness = false;

        /// Language handles the singer declares, and which of them is its default.
        std::vector<std::string> languages;
        std::string defaultLanguage;

        /// Phoneme tokens a lyric may name directly, which no language produces.
        ///
        /// A breath, a glottal stop, a hum: tokens that reach the models because a person wrote
        /// the token itself where a lyric goes. The singer declares them and the loader has
        /// already checked that its models have every one, so this needs no checking here -- a
        /// singer that got it wrong did not load. Feed them to the language layer, which answers
        /// such a word itself instead of asking a language what it means.
        std::vector<std::string> reservedPhonemes;

        /// Whether every stage a synthesis needs is present.
        bool complete() const {
            return acoustic && vocoder;
        }
    };

    /// One singer a loaded package contributes.
    ///
    /// The package fields are repeated on every singer of a package rather than held once beside
    /// them. A singer is what a host names, holds and shows, and making it carry where it came
    /// from costs a few strings and saves every caller a second lookup.
    struct SingerEntry {
        std::string packageId;
        stdc::VersionNumber packageVersion;
        std::string contributionId;
        srt::DisplayText name;
        fs::path packagePath;
        SingerCapabilities capabilities;

        /// What the package says about itself, for a host that lists packages rather than
        /// singers. Empty where the package declares nothing.
        srt::DisplayText packageName;
        srt::DisplayText packageVendor;
        srt::DisplayText packageDescription;
        srt::DisplayText packageCopyright;
        std::string packageUrl;
    };

    /// Returns whether \a interfaceName names an analysis contract that this build runs.
    ///
    /// Analysers are inference modules and therefore share their category with every model that a
    /// voicebank ships; the contract distinguishes them. otter's F0 and Note contracts are
    /// accepted. The Align contract is not accepted, because the editor does not yet run an
    /// aligner.
    bool isAnalysisContract(std::string_view interfaceName);

    /// One analyser a loaded package contributes.
    ///
    /// The same shape as a singer entry because it is found the same way: a package contributes
    /// it, and what it can do is in its declaration. Which contract it answers -- a pitch curve or
    /// a note transcription -- is the interface, so a host picking one filters on that.
    struct AnalyzerEntry {
        std::string packageId;
        stdc::VersionNumber packageVersion;
        std::string contributionId;
        std::string interfaceName;
        std::string variant;
        srt::DisplayText name;

        /// What the editor stores when a person picks this analyser. A path would move when the
        /// package is reinstalled somewhere else; this does not.
        std::string reference() const {
            return AnalyzerReference{packageId, contributionId}.toString();
        }
    };

    /// Why one package could not be opened.
    struct PackageProblem {
        fs::path path;
        std::string reason;
    };

    /// Derives what \a singer can do.
    ///
    /// Reads only the declaration and the exports of what it imports, so it costs no model
    /// loading and can be called while listing voicebanks.
    SingerCapabilities capabilitiesOf(const srt::SingerSpec &singer);

    /// Opens every package directly under each of \a paths and returns the singers they hold.
    ///
    /// The main line has no scanner, so this is it: one directory level, a package is a directory
    /// with a desc.json. A package that will not open is reported in \a problems and skipped, not
    /// fatal -- one broken voicebank must not hide the rest.
    ///
    /// The returned handles keep their packages loaded. The caller owns them and must release
    /// them before the unit goes.
    std::vector<SingerEntry> scan(srt::SynthUnit &unit, const std::vector<fs::path> &paths,
                                  std::vector<srt::PackageHandle> &opened,
                                  std::vector<PackageProblem> &problems);

    /// Returns the analysers among the packages already opened, that is, their inference modules
    /// whose contract isAnalysisContract() accepts.
    ///
    /// Separate from scan() because analysers arrive in their own packages and are not voicebank
    /// content; the same walk finds both, and this reads the other half of what it found.
    std::vector<AnalyzerEntry> analyzersOf(const std::vector<srt::PackageHandle> &opened);

}

#endif // VOICEBANKCATALOG_H
