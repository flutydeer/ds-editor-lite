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

    /// Cuts audio at its silences, so that an analyzer is handed one passage at a time.
    ///
    /// This used to live inside the extraction plugins, one copy per algorithm. It belongs to the
    /// host now: the host is the one that knows which region the user asked about, and an analyzer
    /// that sliced for itself would be deciding that on its behalf.
    ///
    /// The parameters are stated in seconds rather than in frames, which is what lets one setting
    /// serve models running at different rates. The two shipped analyzers wanted 16 kHz and
    /// 44.1 kHz and, converted to time, asked for almost the same thing — a 10 ms hop, a 40 ms
    /// window, a 300 ms gap, half a second of silence kept — differing only in the shortest piece
    /// they would accept. The smaller of the two is used, because cutting finer costs the pitch
    /// analyzer nothing and the note model has a hard ceiling on how much it can encode at once.
    class SlicingProfile {
    public:
        /// Below this RMS a frame counts as silent.
        double threshold = 0.02;

        /// Analysis window, in seconds.
        double window = 0.040;

        /// Distance between adjacent analysis windows, in seconds.
        double hop = 0.010;

        /// Shortest piece worth cutting out, in seconds.
        double minimumLength = 2.0;

        /// Shortest silence worth cutting at, in seconds.
        double minimumInterval = 0.300;

        /// How much silence to leave on each side of a cut, in seconds.
        double maximumSilence = 0.500;

        /// Returns the spans of \a samples that carry sound.
        ///
        /// \a maximumSpan caps how long any one span may be, in seconds; zero means no cap. The
        /// cap is applied after the silence pass and is not advice: a model that refuses a span it
        /// cannot encode, rather than degrading, would otherwise fail on any passage sung without
        /// a breath long enough to cut at. An over-long span is divided into equal pieces rather
        /// than into full ones and a remainder, so that the last piece is not a sliver.
        ///
        /// \a samples must be mono. Returns one span covering everything when there is nothing to
        /// cut, so a caller always has at least one piece of work.
        std::vector<SampleSpan> slice(const std::vector<float> &samples, int sampleRate,
                                      double maximumSpan = 0) const;
    };

}

#endif // AUDIOSLICER_H
