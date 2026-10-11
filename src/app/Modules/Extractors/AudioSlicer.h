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

    /// Splits audio at silences so that an analyzer receives one passage at a time.
    ///
    /// Slicing is performed by the host rather than by each analyzer because the host holds the
    /// region the user selected. An analyzer that sliced its own input would override that
    /// selection.
    ///
    /// The parameters are specified in seconds rather than in frames so that one profile applies to
    /// models with different sample rates. The two shipped analyzers run at 16 kHz and 44.1 kHz.
    /// Converted to time, their slicing parameters are nearly identical (a 10 ms hop, a 40 ms
    /// window, a 300 ms minimum interval and 500 ms of retained silence) and differ only in the
    /// minimum passage length. The smaller minimum is used because finer slicing does not affect
    /// the pitch analyzer, and the note model has a hard limit on the input length per encoding.
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

        /// Returns the spans of \a samples that carry sound.
        ///
        /// \a maximumSpan is the maximum span length in seconds, or zero for no limit. The limit is
        /// applied after the silence pass and is strict. Without it, a model that rejects an input
        /// longer than it can encode would fail on any passage without a silence long enough to
        /// cut at. A span longer than the limit is divided into equal pieces rather than into
        /// full-length pieces and a remainder, so that the last piece is not disproportionately
        /// short.
        ///
        /// \a samples must be mono.
        ///
        /// \return The spans that contain sound. If no cut applies, a single span covering all
        ///         samples, so that the result is never empty.
        std::vector<SampleSpan> slice(const std::vector<float> &samples, int sampleRate,
                                      double maximumSpan = 0) const;
    };

}

#endif // AUDIOSLICER_H
