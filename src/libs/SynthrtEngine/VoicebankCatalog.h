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

/// Package scanning and singer capabilities derived from parsed contributions.
namespace wolf {
    class LinguistSession;
}

namespace lite::synthrt {

    namespace fs = std::filesystem;

    class LanguageBridge;

    /// Speaker IDs belong to the singer; acoustic imports map them to model speaker names.
    struct SpeakerCapability {
        std::string id;

        /// Display name. Defaults to the identifier if the singer declares no name.
        srt::DisplayText name;

        /// Lowest and highest MIDI note intended for this speaker, if the singer declares a range.
        std::optional<std::pair<int, int>> toneRange;
    };

    /// Singer capabilities derived from model imports and exports.
    struct SingerCapabilities {
        bool duration = false;
        bool pitch = false;
        bool variance = false;
        bool acoustic = false;
        bool vocoder = false;

        std::vector<SpeakerCapability> speakers;

        /// Declared host IDs that cannot map to an acoustic speaker, excluded from speakers.
        std::vector<std::string> unaddressableSpeakers;

        /// Variance parameters the models predict.
        std::set<std::string> variancePredictions;

        /// Variance parameters the acoustic model consumes.
        std::set<std::string> varianceControls;

        /// Transition parameters the acoustic model accepts.
        std::set<std::string> transitionControls;

        /// Whether the pitch model accepts an expressiveness curve.
        bool allowsExpressiveness = false;

        /// Model frame widths in seconds. Zero for an absent stage.
        double durationFrameWidth = 0;
        double pitchFrameWidth = 0;
        double varianceFrameWidth = 0;
        double acousticFrameWidth = 0;

        /// Vocoder output sample rate in Hz. Zero if absent.
        int vocoderSampleRate = 0;

        std::vector<std::string> languages;
        std::map<std::string, srt::DisplayText> languageNames;
        std::string defaultLanguage;

        /// Optional default lyrics declared by each language's linguist.
        std::map<std::string, std::string> defaultLyrics;

        /// Singer-declared tokens that bypass G2P; the loader validates model support.
        std::vector<std::string> reservedPhonemes;

        /// Intersection of nonempty model phoneme tables, sorted and unique, without language
        /// prefixes.
        /// No value means unknown coverage; an empty vector means no common phonemes.
        std::optional<std::vector<std::string>> singablePhonemes;

        bool complete() const {
            return acoustic && vocoder;
        }
    };

    /// Metadata includes packages without singers.
    struct PackageEntry {
        std::string id;
        stdc::VersionNumber version;
        fs::path path;
        srt::DisplayText vendor;
        srt::DisplayText description;
        srt::DisplayText copyright;
        std::string url;
    };

    struct SingerEntry {
        std::string packageId;
        stdc::VersionNumber packageVersion;
        std::string contributionId;
        srt::DisplayText name;
        fs::path packagePath;
        SingerCapabilities capabilities;

        srt::DisplayText packageName;
        srt::DisplayText packageVendor;
        srt::DisplayText packageDescription;
        srt::DisplayText packageCopyright;
        std::string packageUrl;

        /// Conversion readiness captured with this catalog after supplying phoneme coverage.
        std::set<std::string> convertibleLanguages;
    };

    /// Whether the interface is a supported otter analysis contract.
    bool isAnalysisContract(std::string_view interfaceName);

    struct AnalyzerEntry {
        std::string packageId;
        stdc::VersionNumber packageVersion;
        std::string contributionId;
        std::string interfaceName;
        std::string variant;
        srt::DisplayText name;

        /// Analyzer-declared language choices and default, empty for language-independent
        /// analyzers.
        std::vector<std::string> languages;
        std::string defaultLanguage;

        /// Use the requested language when supported, otherwise the declared default.
        /// Language-independent analyzers preserve the request.
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

        std::string reference() const {
            return AnalyzerReference{packageId, contributionId}.toString();
        }
    };

    struct PackageProblem {
        fs::path path;
        std::string reason;
    };

    /// Read capabilities without opening models.
    SingerCapabilities capabilitiesOf(const srt::SingerSpec &singer);

    /// Supply declared phoneme inventories after rebuilding the language catalog.
    /// Skip unknown inventories and singers without languages; retain known empty inventories.
    void feedSingerPhonemes(wolf::LinguistSession &session, const std::vector<SingerEntry> &catalog);
    void feedSingerPhonemes(LanguageBridge &language, const std::vector<SingerEntry> &catalog);

    /// Scan package roots and their immediate children, reporting failed packages in problems.
    /// The caller must release the returned package handles before destroying the unit.
    std::vector<SingerEntry> scan(srt::SynthUnit &unit, const std::vector<fs::path> &paths,
                                  std::vector<srt::PackageHandle> &opened,
                                  std::vector<PackageProblem> &problems);

    /// List supported analyzer contributions from the opened packages.
    std::vector<AnalyzerEntry> analyzersOf(const std::vector<srt::PackageHandle> &opened);

}

#endif // VOICEBANKCATALOG_H
