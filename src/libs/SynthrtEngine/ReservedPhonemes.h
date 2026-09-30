#ifndef RESERVEDPHONEMES_H
#define RESERVEDPHONEMES_H

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace lite::synthrt {

    /// The phonemes that the editor always treats as reserved, independently of the voicebank.
    ///
    /// A reserved phoneme is a token that a lyric may contain directly and that no language
    /// produces. A singer declares its reserved phonemes in the reservedPhonemes field of its
    /// declaration, which synthrt exposes as SingerSpec::reservedPhonemes(). The editor
    /// additionally reserves SP (silence) and AP (breath), because the project model and the
    /// inference input depend on them. A task uses the union of both sets, which
    /// SynthrtEngine::reservedPhonemesOf() returns.
    inline constexpr std::array<std::string_view, 2> FORCED_RESERVED_PHONEMES{"SP", "AP"};

    /// The reserved phoneme that pads a phrase, and fills a gap between notes, with silence.
    inline constexpr std::string_view SILENCE_PHONEME = FORCED_RESERVED_PHONEMES[0];

    /// Returns the forced reserved phonemes followed by those of \a declared that are not among
    /// them, in their declared order.
    inline std::vector<std::string> withForcedReservedPhonemes(
        const std::vector<std::string> &declared) {
        std::vector<std::string> result(FORCED_RESERVED_PHONEMES.begin(),
                                        FORCED_RESERVED_PHONEMES.end());
        for (const auto &phoneme : declared) {
            if (std::find(result.begin(), result.end(), phoneme) == result.end()) {
                result.push_back(phoneme);
            }
        }
        return result;
    }

}

#endif // RESERVEDPHONEMES_H
