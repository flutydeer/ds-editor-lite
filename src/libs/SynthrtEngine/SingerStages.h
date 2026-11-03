#ifndef SINGERSTAGES_H
#define SINGERSTAGES_H

#include <string_view>

namespace lite::synthrt {

    enum class SingerStage {
        Duration,
        Pitch,
        Variance,
        Acoustic,
        Vocoder,
    };

    /// Import roles defined by the DiffSinger singer contract.
    constexpr std::string_view roleOf(SingerStage stage) {
        switch (stage) {
            case SingerStage::Duration:
                return "singer/duration";
            case SingerStage::Pitch:
                return "singer/pitch";
            case SingerStage::Variance:
                return "singer/variance";
            case SingerStage::Acoustic:
                return "singer/acoustic";
            case SingerStage::Vocoder:
                break;
        }
        return "singer/vocoder";
    }

}

#endif // SINGERSTAGES_H
