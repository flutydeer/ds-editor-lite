#ifndef AUDIOCLIPGRAPHICSITEM_H
#define AUDIOCLIPGRAPHICSITEM_H

#include "AbstractClipView.h"
#include "Global/AppGlobal.h"
#include "UI/Utils/WaveformRenderUtils.h"
#include "UI/Views/TrackEditor/AudioWaveformSampler.h"

#include <lite/MusicBase/Timeline.h>
#include <lite/ProjectModel/AppModel/AudioInfoModel.h>

// Graphics item that displays an audio clip's waveform in the track editor.
//
// Rendering modes (selected automatically by zoom level):
//   1. Peak mode — uses pre-computed peakCache / peakCacheMipmap (vertical lines)
//   2. Sub-chunk peak mode — reads raw samples via IO, computes per-pixel min/max
//   3. Waveform curve mode — reads raw samples, applies Lanczos sinc interpolation,
//      draws a smooth continuous curve via QPainterPath
//
// The drawing uses physical-pixel stepping and a scene-aligned sampling grid to
// ensure correct HiDPI rendering and jitter-free display during clip trimming.
class AudioClipView final : public AbstractClipView {
public:
    [[nodiscard]] ClipType clipType() const override {
        return Audio;
    }

    explicit AudioClipView(int itemId, QGraphicsItem *parent = nullptr);
    ~AudioClipView() override;

    void setRenderMode(WaveformRenderUtils::Mode mode);
    [[nodiscard]] WaveformRenderUtils::Mode renderMode() const;

    [[nodiscard]] QString path() const;
    void setPath(const QString &path);
    void setTimeline(const Timeline &timeline);
    void setAudioInfo(const AudioInfoModel &info);
    void setStatus(AppGlobal::AudioLoadStatus status);
    void setErrorMessage(const QString &errorMessage);
    [[nodiscard]] int contentLength() const override;

private:
    void drawPreviewArea(QPainter *painter, const QRectF &previewRect, QColor color) override;
    [[nodiscard]] QString clipTypeName() const override;
    [[nodiscard]] QString iconPath() const override;

    AppGlobal::AudioLoadStatus m_status = AppGlobal::Init;
    AudioInfoModel m_audioInfo;
    QString m_errorMessage;
    Timeline m_timeline;
    QString m_path;

    WaveformRenderUtils::Mode m_renderMode = WaveformRenderUtils::FilledMode;
    AudioWaveformSampler m_waveformSampler;
};

#endif // AUDIOCLIPGRAPHICSITEM_H
