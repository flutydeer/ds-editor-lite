#ifndef SINGERSTAGES_H
#define SINGERSTAGES_H

#include <string_view>

namespace lite::synthrt {

    /// The five stages of a DiffSinger singer.
    enum class SingerStage {
        Duration,
        Pitch,
        Variance,
        Acoustic,
        Vocoder,
    };

    /// Returns the import role through which a singer declaration names the model of \a stage.
    ///
    /// The roles belong to the DiffSinger singer contract of dsinfer, which does not export them
    /// as constants. They are therefore defined once here for every consumer in the editor: the
    /// catalog that derives capabilities, the pipeline that returns import options and the
    /// inference tasks.
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
