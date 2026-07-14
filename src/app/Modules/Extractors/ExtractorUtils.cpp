#include "ExtractorUtils.h"

#include <synthrt/Audio/AudioPipeline.h>
#include <synthrt/Audio/ResampleConfig.h>

#include <QObject>

namespace ExtractorUtils {

    std::optional<DecodedAudio> decodeAudio(const QString &path,
                                            const std::function<bool()> &isCancelled,
                                            QString &errorMessage) {
        const auto utf8Path = path.toUtf8().toStdString();
        auto pipeline = srt::audio::AudioPipeline::create();

        if (isCancelled()) {
            return std::nullopt;
        }
        auto infoExp = pipeline.probe(utf8Path);
        if (!infoExp) {
            errorMessage = QObject::tr("Failed to probe audio file: ") +
                           QString::fromUtf8(infoExp.error().message());
            return std::nullopt;
        }
        auto info = infoExp.take();

        if (isCancelled()) {
            return std::nullopt;
        }
        auto bufferExp = pipeline.decodeAndResample(
            utf8Path, srt::audio::ResampleConfig::forMonoFloat(info.sampleRate));
        if (!bufferExp) {
            errorMessage = QObject::tr("Failed to decode audio: ") +
                           QString::fromUtf8(bufferExp.error().message());
            return std::nullopt;
        }
        if (isCancelled()) {
            return std::nullopt;
        }

        return DecodedAudio{bufferExp.take(), info.sampleRate};
    }

} // namespace ExtractorUtils
