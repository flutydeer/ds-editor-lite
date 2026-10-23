#ifndef AUDIOSLICER_H
#define AUDIOSLICER_H

#include <cstdint>
#include <vector>

namespace Extractors {

    /// A half-open span of mono samples.
    struct SampleSpan {
        int64_t begin = 0;
        int64_t end = 0;

        int64_t length() const {
            return end - begin;
        }
    };

    /// Split mono audio at silences. Profile durations are in seconds.
    class SlicingProfile {
    public:
        /// RMS level below which a frame is silent.
        double threshold = 0.02;

        /// Analysis window, in seconds.
        double window = 0.040;

        /// Distance between adjacent analysis windows, in seconds.
        double hop = 0.010;

        /// Minimum passage length, in seconds.
        double minimumLength = 2.0;

        /// Minimum silence length at which a cut is made, in seconds.
        double minimumInterval = 0.300;

        /// Maximum silence retained on each side of a cut, in seconds.
        double maximumSilence = 0.500;

        /// Return half-open spans of mono samples. maximumSpan is in seconds, zero for no limit.
        /// Split overlong passages evenly after silence detection.
        std::vector<SampleSpan> slice(const std::vector<float> &samples, int sampleRate,
                                      double maximumSpan = 0) const;
    };

}

#endif // AUDIOSLICER_H
