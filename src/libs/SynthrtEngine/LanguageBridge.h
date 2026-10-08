#ifndef LANGUAGEBRIDGE_H
#define LANGUAGEBRIDGE_H

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "PronunciationStage.h"

#include <synthrt/Core/SynthUnit.h>
#include <synthrt/Support/Expected.h>

#include <stdcorelib/support/versionnumber.h>

namespace lite::synthrt {

    /// Adapter that provides the editor's language operations through one wolf linguist session.
    ///
    /// The session replaces the separate LanguageService, G2P Manager singleton, LanguageRoute and
    /// S2P LanguageResource of the refactor branch. The session maintains its own catalog,
    /// readiness state and pool, so this class has no two-stage arrangement, no deferred model
    /// loading and no warm-up pass.
    ///
    /// This class retains the editor's conventions: singers are identified as in the project
    /// model, and each conversion covers one language because the editor batches per language.
    class LanguageBridge {
    public:
        /// Address of a loaded singer in the language layer.
        ///
        /// The version is a required part of the address. The specification allows two versions
        /// of a package to be loaded at the same time, and a module reference cannot carry a
        /// version because it binds to the version that the dependency graph resolved. A package
        /// identifier and a contribution identifier therefore do not identify a unique loaded
        /// singer, and a lookup without a version succeeds only while exactly one version is
        /// installed.
        struct Singer {
            std::string packageId;
            std::string contributionId;
            stdc::VersionNumber version;
        };

        /// Input word of a conversion. The fields correspond to the per-note data of the editor.
        struct Word {
            std::string lyric;
            /// A pronunciation entered by the user, which the conversion must not overwrite.
            std::optional<std::string> pronunciation;
            /// A phoneme sequence edited by the user. Set together with \c onsets or not at all.
            std::optional<std::vector<std::string>> phonemes;
            std::optional<std::vector<bool>> onsets;
        };

        /// Conversion result of one word.
        struct Result {
            std::string pronunciation;
            std::vector<std::string> candidates;
            std::vector<std::string> phonemes;
            std::vector<bool> onsets;
            /// Empty if the word was converted; otherwise the failure reason for display.
            std::string error;
            /// How the pronunciation was produced: a dictionary entry, a model, a rule or the last
            /// resort. Diagnostic only, and Unspecified when the language module does not report it.
            PronunciationStage stage = PronunciationStage::Unspecified;
        };

        /// Final stage of a conversion.
        enum class Depth {
            /// Lyric to pronunciation, as required by the lyric filling dialog.
            Pronunciation,
            /// Lyric to phonemes, as required by synthesis.
            Phonemes,
            /// Lyric to phonemes and onsets.
            Onsets,
        };

        explicit LanguageBridge(srt::SynthUnit &unit);
        ~LanguageBridge();

        /// Rebuilds the catalog from the packages the unit has committed.
        ///
        /// Called after voicebanks are scanned. The call is inexpensive and may be repeated
        /// whenever the voicebanks change. Holders of an earlier catalog continue to see its
        /// contents.
        void refresh();

        /// Releases the session state of \a singer, including its package handle, so that the
        /// package can be unloaded. refresh() must be called after the subsequent rescan.
        void release(const Singer &singer);

        /// Returns the languages that \a singer declares, in the iteration order of its language
        /// map.
        std::vector<std::string> languagesOf(const Singer &singer) const;

        /// Returns whether \a singer has a conversion route for \a language. Loads nothing.
        bool canConvert(const Singer &singer, const std::string &language) const;

        /// Returns the reason \a singer cannot convert \a language, or an empty string when the
        /// route exists. Loads nothing; mirrors canConvert().
        std::string unavailableReason(const Singer &singer, const std::string &language) const;

        /// Returns the deepest layer that \a singer reaches for \a language. Loads nothing; mirrors
        /// canConvert().
        ///
        /// The limit belongs to the composition, not to a failure: a composition without an Onset
        /// member reaches Phonemes, and one without an S2P member reaches Pronunciation. A caller
        /// that needs phonemes reads this value before it converts, because a request for a deeper
        /// layer returns the shallower result without an error, and nothing in that result
        /// distinguishes "this language has no onset layer" from "no phoneme begins this syllable".
        ///
        /// Empty if the route is not usable, which is the state canConvert() reports as false; the
        /// reason is then read from unavailableReason(). The two states are distinguished instead of
        /// being folded into Pronunciation, so that a caller does not treat an unusable route as a
        /// language without an onset layer and skip the conversion that would report the failure.
        std::optional<Depth> maxDepth(const Singer &singer, const std::string &language) const;

        /// Registers the phonemes that \a singer supports, so that the session can report coverage.
        ///
        /// wolf does not read voicebank formats, so the list comes from the editor's reading of
        /// the singer. Without the list, coverage is Unknown, which differs from zero coverage.
        void setSingerPhonemes(const Singer &singer, std::vector<std::string> phonemes);

        /// Sets the reserved markers of every singer that declares no reserved phonemes.
        ///
        /// A reserved marker bypasses grapheme-to-phoneme conversion. Its result is the marker
        /// itself as pronunciation, one phoneme and one onset. The default set is SP and AP. A
        /// voicebank with additional markers, such as a breath, a glottal stop or a hum, declares
        /// them in the reservedPhonemes field of the singer category, which the language session
        /// reads from the declaration. This set is the fallback for a singer without that field.
        ///
        /// \note Reserved markers are also excluded from a linguist's declared phoneme inventory.
        ///       A marker set that differs from the set against which a voicebank was packaged
        ///       therefore appears as a coverage gap rather than as silence.
        void setReservedMarkers(std::vector<std::string> markers);
        std::vector<std::string> reservedMarkers() const;

        /// Converts a batch of words in one language.
        ///
        /// The call fails as a whole only if the route does not exist. A word that cannot be
        /// converted keeps its position in the batch and carries its own error, so that one
        /// unknown word in a lyric sheet does not prevent the conversion of the other words.
        srt::Expected<std::vector<Result>> convert(const Singer &singer,
                                                   const std::string &language,
                                                   const std::vector<Word> &words,
                                                   Depth depth = Depth::Onsets) const;

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // LANGUAGEBRIDGE_H
