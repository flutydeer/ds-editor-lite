#include "InferFrameLayout.h"

#include <lite/SynthrtEngine/SynthrtEngine.h>

namespace InferFrameLayout {

    double inputInterval(const SingerIdentifier &identifier,
                         const lite::synthrt::SingerStage stage) {
        using lite::synthrt::SingerStage;
        const auto singer = SynthrtEngine::instance().singer(identifier);
        if (!singer)
            return kDefaultInputInterval;
        const auto &capabilities = singer->capabilities;
        const double width = [&] {
            switch (stage) {
                case SingerStage::Duration:
                    return capabilities.durationFrameWidth;
                case SingerStage::Pitch:
                    return capabilities.pitchFrameWidth;
                case SingerStage::Variance:
                    return capabilities.varianceFrameWidth;
                case SingerStage::Acoustic:
                case SingerStage::Vocoder:
                    break;
            }
            return capabilities.acousticFrameWidth;
        }();
        return width > 0 ? width : kDefaultInputInterval;
    }

    int vocoderSampleRate(const SingerIdentifier &identifier) {
        const auto singer = SynthrtEngine::instance().singer(identifier);
        return singer ? singer->capabilities.vocoderSampleRate : 0;
    }

}
