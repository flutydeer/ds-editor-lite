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

    /// Mono analyzer input with file-relative timing.
    struct PreparedAudio {
        std::vector<float> samples;

        int sampleRate = 0;

        /// File-relative position of the first sample, in milliseconds.
        double startMs = 0;
    };

    /// Decode the file interval [startMs, endMs) and resample to the analyzer's sampleRate.
    /// Return no value on failure or cancellation; error is empty only for cancellation.
    std::optional<PreparedAudio> prepareAudio(talcs::AbstractAudioFormatIO *io, double startMs,
                                              double endMs, int sampleRate,
                                              const std::function<bool()> &cancelled,
                                              QString &error);

}

#endif // ANALYSISAUDIO_H
