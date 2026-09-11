#ifndef ANALYSISAUDIO_H
#define ANALYSISAUDIO_H

#include <functional>
#include <optional>
#include <vector>

#include <QString>

namespace talcs {
    class AbstractAudioFormatIO;
}

namespace Extractors {

    /// Mono audio at the rate an analyzer asked for, and where it sits on the project timeline.
    struct PreparedAudio {
        /// Mono samples.
        ///
        /// Mono because every analyzer shipped so far wants one channel and would average the
        /// rest itself. Averaging here instead avoids carrying the other channels through a
        /// resample and an interleave first, and costs nothing: the arithmetic is the same.
        std::vector<float> samples;

        /// The rate the samples are at, which is the rate that was asked for.
        int sampleRate = 0;

        /// Where the first sample sits on the project timeline, in milliseconds.
        double startMs = 0;
    };

    /// Decodes part of an audio file and resamples it to \a sampleRate.
    ///
    /// The rate is a parameter rather than a constant because analyzers disagree about it — the
    /// pitch model wants 16 kHz and the note model 44.1 kHz — so the caller reads it from the
    /// analyzer's own declaration instead of assuming one.
    ///
    /// \a startMs and \a endMs select the region, on the file's own timeline. \a cancelled is
    /// polled while reading; an empty result with an empty \a error means it was cancelled.
    ///
    /// Nothing here validates the result against what the analyzer will accept. That check lives
    /// in the analyzer, which is the only thing that knows, and duplicating it here is how the two
    /// would come to disagree.
    std::optional<PreparedAudio> prepareAudio(talcs::AbstractAudioFormatIO *io, double startMs,
                                              double endMs, int sampleRate,
                                              const std::function<bool()> &cancelled,
                                              QString &error);

}

#endif // ANALYSISAUDIO_H
