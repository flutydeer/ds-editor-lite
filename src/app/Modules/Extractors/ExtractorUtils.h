#ifndef EXTRACTORUTILS_H
#define EXTRACTORUTILS_H

#include <functional>
#include <optional>

#include <QString>

#include <synthrt/Audio/AudioBuffer.h>

namespace ExtractorUtils {

    struct DecodedAudio {
        srt::audio::AudioBuffer buffer;
        int sampleRate = 0;
    };

    std::optional<DecodedAudio> decodeAudio(const QString &path,
                                            const std::function<bool()> &isCancelled,
                                            QString &errorMessage);

} // namespace ExtractorUtils

#endif // EXTRACTORUTILS_H
