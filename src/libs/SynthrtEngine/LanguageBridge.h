#ifndef LANGUAGEBRIDGE_H
#define LANGUAGEBRIDGE_H

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <synthrt/Core/SynthUnit.h>
#include <synthrt/Support/Expected.h>

#include <stdcorelib/support/versionnumber.h>

#include <wolf/Session/LinguistSession.h>

namespace lite::synthrt {

    /// The editor's language needs, answered by wolf.
    ///
    /// The refactor line had a LanguageService, a G2P Manager singleton, a LanguageRoute and an
    /// S2P LanguageResource, and the editor asked each of them separately. wolf answers all of it
    /// from one session, so this is a good deal smaller than what it replaces: the two stage
    /// arrangement, the deferred model loading and the warm-up pass all go, because the session
    /// already keeps a catalogue, a readiness answer and a pool of its own.
    ///
    /// What stays on this side is the editor's vocabulary: singers are named the way the project
    /// names them, and a conversion is per language because that is how the editor batches.
    class LanguageBridge {
    public:
        /// Which singer, as the language layer addresses one.
        ///
        /// The version is part of the address and not decoration. The specification lets two
        /// versions of one package be loaded at once, and a module reference deliberately cannot
        /// carry a version -- it binds to whatever the dependency graph resolved. So a package
        /// identifier and a contribution identifier do not name one loaded singer, and asking
        /// with the version left out is answered only while exactly one version is installed.
        struct Singer {
            std::string packageId;
            std::string contributionId;
            stdc::VersionNumber version;
        };

        /// One word on the way in. Mirrors what the editor holds per note.
        struct Word {
            std::string lyric;
            /// A pronunciation the user typed, which must not be overwritten.
            std::optional<std::string> pronunciation;
            /// A phoneme layer the user edited, with its onsets. Both or neither.
            std::optional<std::vector<std::string>> phonemes;
            std::optional<std::vector<bool>> onsets;
        };

        /// What one word converted to.
        struct Result {
            std::string pronunciation;
            std::vector<std::string> candidates;
            std::vector<std::string> phonemes;
            std::vector<bool> onsets;
            /// Empty when the word converted. Otherwise why it did not, for the editor to show.
            std::string error;
        };

        /// How far a conversion should go.
        enum class Depth {
            /// Lyric to pronunciation. What the lyric filling dialog needs.
            Pronunciation,
            /// And on to phonemes. What a synthesis needs.
            Phonemes,
            /// And on to onsets.
            Onsets,
        };

        explicit LanguageBridge(srt::SynthUnit &unit);
        ~LanguageBridge();

        /// Rebuilds the catalogue from the packages the unit has committed.
        ///
        /// Called after voicebanks are scanned. Cheap enough to call again whenever they change;
        /// holders of an earlier catalogue keep seeing the earlier answers.
        void refresh();

        /// The languages a singer declares, in the order its map yields them.
        std::vector<std::string> languagesOf(const Singer &singer) const;

        /// Whether a singer can convert this language at all, without loading anything.
        bool canConvert(const Singer &singer, const std::string &language) const;

        /// Tells the session which phonemes a singer can sing, so coverage can be reported.
        ///
        /// wolf does not read voicebank formats, so this comes from the editor's own reading of
        /// the singer. Without it coverage is Unknown, which is not the same as zero.
        void setSingerPhonemes(const Singer &singer, std::vector<std::string> phonemes);

        /// The markers a word may be for any singer that declares none of its own, which are
        /// answered here rather than converted.
        ///
        /// A marker never reaches grapheme-to-phoneme: it comes back as its own pronunciation,
        /// one phoneme, one onset. Defaults to the ecosystem's SP and AP. A voicebank that brings
        /// more of its own -- a breath, a glottal stop, a hum -- declares them as the singer
        /// category's reservedPhonemes, which the language session reads from the declaration
        /// itself; this set is only the fallback for a singer that declares none.
        ///
        /// \note Reserved markers are also kept out of a linguist's declared phoneme inventory,
        ///       so a marker set that disagrees with the one a voicebank was packaged against
        ///       shows up as a coverage gap rather than as silence.
        void setReservedMarkers(std::vector<std::string> markers);
        std::vector<std::string> reservedMarkers() const;

        /// Converts one batch, all in one language.
        ///
        /// Fails as a whole only when the route does not exist; a word that could not be converted
        /// keeps its place in the batch and carries its own error, because a lyric sheet with one
        /// unknown word should still fill in the rest.
        srt::Expected<std::vector<Result>> convert(const Singer &singer,
                                                   const std::string &language,
                                                   const std::vector<Word> &words,
                                                   Depth depth = Depth::Onsets) const;

        /// Asks any conversion in flight to stop.
        void cancel();

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // LANGUAGEBRIDGE_H
