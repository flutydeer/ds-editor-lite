#ifndef INFERENCE_FLAG_H
#define INFERENCE_FLAG_H

#include "Utils/BitMaskFlag.h"

namespace InferenceFlag {
    using Type = BitMaskFlag<struct InferenceFlagTag, uint8_t>;

    inline constexpr Type None(0);
    inline constexpr Type Duration(1 << 0);
    inline constexpr Type Pitch(1 << 1);
    inline constexpr Type Variance(1 << 2);
    inline constexpr Type Acoustic(1 << 3);
    inline constexpr Type Vocoder(1 << 4);
    inline constexpr Type All(Duration | Pitch | Variance | Acoustic | Vocoder);
}

#endif // INFERENCE_FLAG_H