#ifndef AUDIOWAVEFORMSAMPLER_H
#define AUDIOWAVEFORMSAMPLER_H

#include "UI/Utils/WaveformRenderUtils.h"

#include <lite/MusicBase/Timeline.h>
#include <lite/ProjectModel/AppModel/AudioInfoModel.h>

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

namespace talcs {
    class AbstractAudioFormatIO;
}

class AudioWaveformSampler final {
public:
    enum class Geometry { None, FilledPeaks, VerticalPeaks, Curve };

    struct Result {
        Geometry geometry = Geometry::None;
        QVector<WaveformRenderUtils::PeakPoint> peaks;
        QVector<QPointF> curve;
        QVector<QPointF> sampleDots;
        double sampleDotRadius = 0.0;
    };

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
    [[nodiscard]] Result sample(const Request &request);

private:
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
};

#endif // AUDIOWAVEFORMSAMPLER_H
