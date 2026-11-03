#ifndef INFERFRAMELAYOUT_H
#define INFERFRAMELAYOUT_H

#include <lite/ProjectModel/AppModel/SingerIdentifier.h>
#include <lite/SynthrtEngine/SingerStages.h>

namespace InferFrameLayout {

    /// Fallback frame interval in seconds.
    inline constexpr double kDefaultInputInterval = 0.01;

    /// Sample input curves at the stage model's frame interval.
    double inputInterval(const SingerIdentifier &identifier, lite::synthrt::SingerStage stage);

    /// Vocoder output sample rate, or zero when unknown.
    int vocoderSampleRate(const SingerIdentifier &identifier);

}

#endif // INFERFRAMELAYOUT_H
