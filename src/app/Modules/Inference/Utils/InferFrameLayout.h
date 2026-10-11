#ifndef INFERFRAMELAYOUT_H
#define INFERFRAMELAYOUT_H

#include <lite/ProjectModel/AppModel/SingerIdentifier.h>
#include <lite/SynthrtEngine/SingerStages.h>

/// Time grids of the inference input and output, as defined by the models of a singer.
namespace InferFrameLayout {

    /// Frame interval, in seconds, of an input curve if the singer declares no frame width for
    /// the stage, for example because the singer is not in the catalog.
    inline constexpr double kDefaultInputInterval = 0.01;

    /// Returns the interval at which the input curves of \a stage are sampled, which is the frame
    /// width of the model of that stage. The model therefore receives its curves on its own
    /// frames, and the inference library does not resample them a second time.
    double inputInterval(const SingerIdentifier &identifier, lite::synthrt::SingerStage stage);

    /// Returns the output sample rate of the vocoder of \a identifier, or zero if the catalog
    /// does not declare it.
    int vocoderSampleRate(const SingerIdentifier &identifier);

}

#endif // INFERFRAMELAYOUT_H
