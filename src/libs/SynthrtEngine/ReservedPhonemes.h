#ifndef RESERVEDPHONEMES_H
#define RESERVEDPHONEMES_H

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace lite::synthrt {

    /// SP and AP are always reserved. Singer-declared markers are added by reservedPhonemesOf().
    inline constexpr std::array<std::string_view, 2> FORCED_RESERVED_PHONEMES{"SP", "AP"};

    /// Silence token used for phrase padding and gaps.
    inline constexpr std::string_view SILENCE_PHONEME = FORCED_RESERVED_PHONEMES[0];

    /// Merge declared markers after the forced markers, preserving order and removing duplicates.
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
