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

    /// Adapts per-language editor batches to a wolf linguist session.
    class LanguageBridge {
    public:
        /// Identifies a loaded singer, including its package version.
        struct Singer {
            std::string packageId;
            std::string contributionId;
            stdc::VersionNumber version;
        };

        struct Word {
            std::string lyric;
            /// A pronunciation entered by the user, which the conversion must not overwrite.
            std::optional<std::string> pronunciation;
            /// A phoneme sequence edited by the user. Set together with \c onsets or not at all.
            std::optional<std::vector<std::string>> phonemes;
            std::optional<std::vector<bool>> onsets;
        };

        struct Result {
            std::string pronunciation;
            std::vector<std::string> candidates;
            std::vector<std::string> phonemes;
            std::vector<bool> onsets;
            /// Empty if the word was converted; otherwise the failure reason for display.
            std::string error;
            /// Diagnostic hit stage, or Unspecified when the module does not report it.
            PronunciationStage stage = PronunciationStage::Unspecified;
        };

        enum class Depth {
            Pronunciation,
            Phonemes,
            Onsets,
        };

        explicit LanguageBridge(srt::SynthUnit &unit);
        ~LanguageBridge();

        /// Publish a language catalog after scanning packages. Existing snapshots remain valid.
        void refresh();

        /// Release the singer's session state and package handle. Call refresh() after rescanning.
        void release(const Singer &singer);

        std::vector<std::string> languagesOf(const Singer &singer) const;

        /// Query route availability without loading models.
        bool canConvert(const Singer &singer, const std::string &language) const;

        /// Failure reason, or an empty string for an available route.
        std::string unavailableReason(const Singer &singer, const std::string &language) const;

        /// Deepest available conversion layer, or no value for an unusable route.
        /// Check before conversion: a deeper request may return a shallower result without error.
        std::optional<Depth> maxDepth(const Singer &singer, const std::string &language) const;

        /// Set the singer's phoneme inventory. An unspecified inventory means unknown coverage.
        void setSingerPhonemes(const Singer &singer, std::vector<std::string> phonemes);

        /// Fallback markers for singers without reservedPhonemes, initially SP and AP.
        /// Markers bypass G2P and are excluded from the linguist's ordinary phoneme inventory.
        void setReservedMarkers(std::vector<std::string> markers);
        std::vector<std::string> reservedMarkers() const;

        /// Convert one language batch. Per-word failures retain their positions and carry an error.
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
