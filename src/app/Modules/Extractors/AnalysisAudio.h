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

    /// Mono audio at the sample rate an analyzer requires, with its position on the project
    /// timeline.
    struct PreparedAudio {
        /// Mono samples.
        ///
        /// Every analyzer currently shipped accepts one channel and would otherwise average the
        /// channels itself. Averaging here avoids carrying the other channels through resampling
        /// and interleaving, and the arithmetic is identical.
        std::vector<float> samples;

        /// Sample rate of \c samples, equal to the requested rate.
        int sampleRate = 0;

        /// Position of the first sample on the project timeline, in milliseconds.
        double startMs = 0;
    };

    /// Decodes part of an audio file and resamples it to \a sampleRate.
    ///
    /// The sample rate is a parameter rather than a constant because analyzers require different
    /// rates: the pitch model requires 16 kHz and the note model 44.1 kHz. The caller therefore
    /// reads the rate from the analyzer's declaration.
    ///
    /// \a startMs and \a endMs select the region on the file's timeline. \a cancelled is polled
    /// while reading.
    ///
    /// The result is not validated against the analyzer's input constraints. That check belongs to
    /// the analyzer, and a duplicate here could diverge from it.
    ///
    /// \return The prepared audio on success. \c std::nullopt with a non-empty \a error on failure,
    ///         or with an empty \a error if the operation was cancelled.
    std::optional<PreparedAudio> prepareAudio(talcs::AbstractAudioFormatIO *io, double startMs,
                                              double endMs, int sampleRate,
                                              const std::function<bool()> &cancelled,
                                              QString &error);

}

#endif // ANALYSISAUDIO_H
