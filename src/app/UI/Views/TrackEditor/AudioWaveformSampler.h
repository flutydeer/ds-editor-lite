#ifndef AUDIOWAVEFORMSAMPLER_H
#define AUDIOWAVEFORMSAMPLER_H

#include "UI/Utils/WaveformRenderUtils.h"

#include <lite/MusicBase/Timeline.h>
#include <lite/ProjectModel/AppModel/AudioInfoModel.h>

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

#include <optional>

namespace talcs {
    class AbstractAudioFormatIO;
}

class AudioWaveformSampler final {
public:
    using Geometry = WaveformRenderUtils::Geometry;
    using Result = WaveformRenderUtils::SampledWaveform;

    struct Request {
        const AudioInfoModel *audioInfo = nullptr;
        const Timeline *timeline = nullptr;
        int materialStartTick = 0;
        int visibleStartTick = 0;
        QRectF previewSceneRect;
        QRectF visibleSceneRect;
        double horizontalScale = 1.0;
        double pixelsPerQuarterNote = 64.0;
        double leftMarginPx = 0.0;
        double devicePixelRatio = 1.0;
    };

    AudioWaveformSampler() = default;
    ~AudioWaveformSampler();

    AudioWaveformSampler(const AudioWaveformSampler &) = delete;
    AudioWaveformSampler &operator=(const AudioWaveformSampler &) = delete;

    void setPath(const QString &path);
    void invalidate();
    [[nodiscard]] Result sample(const Request &request);

private:
    struct CacheKey {
        const AudioInfoModel *audioInfo = nullptr;
        const Timeline *timeline = nullptr;
        const void *peakCacheData = nullptr;
        const void *peakCacheMipmapData = nullptr;
        int materialStartTick = 0;
        int visibleStartTick = 0;
        int chunkSize = 0;
        int mipmapScale = 0;
        int sampleRate = 0;
        int channels = 0;
        long long frames = 0;
        qsizetype peakCacheSize = 0;
        qsizetype peakCacheMipmapSize = 0;
        QRectF previewSceneRect;
        QRectF visibleSceneRect;
        double horizontalScale = 1.0;
        double pixelsPerQuarterNote = 64.0;
        double leftMarginPx = 0.0;
        double devicePixelRatio = 1.0;

        bool operator==(const CacheKey &) const = default;
    };

    [[nodiscard]] static CacheKey makeCacheKey(const Request &request);
    [[nodiscard]] bool ensureIO();
    void resetIO();
    [[nodiscard]] double tickToSamplePos(const Request &request, double tick) const;
    [[nodiscard]] double samplePosToTick(const Request &request, double samplePos) const;
    [[nodiscard]] Result samplePeakMode(const Request &request) const;
    [[nodiscard]] Result sampleSubChunkPeakMode(const Request &request);
    [[nodiscard]] Result sampleCurveMode(const Request &request);

    static double sincInterpolate(const QVector<float> &samples, qint64 offset, qint64 totalFrames,
                                  double position);

    QString m_path;
    talcs::AbstractAudioFormatIO *m_io = nullptr;
    QVector<float> m_ioBuffer;
    std::optional<CacheKey> m_cacheKey;
    Result m_cachedResult;
};

#endif // AUDIOWAVEFORMSAMPLER_H
