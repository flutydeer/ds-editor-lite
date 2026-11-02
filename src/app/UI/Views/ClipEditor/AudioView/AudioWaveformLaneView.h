#ifndef AUDIOWAVEFORMLANEVIEW_H
#define AUDIOWAVEFORMLANEVIEW_H

#include "Global/AppGlobal.h"
#include "UI/Views/Common/AbstractGraphicsRectItem.h"
#include "UI/Views/TrackEditor/AudioWaveformSampler.h"

#include <lite/MusicBase/Timeline.h>
#include <lite/ProjectModel/AppModel/AudioInfoModel.h>

#include <QColor>
#include <QString>

// One channel lane of the audio view. Draws the waveform of a single channel of
// the audio material, so a stereo clip shows two independent waveforms instead
// of one downmix.
class AudioWaveformLaneView final : public AbstractGraphicsRectItem {
    Q_OBJECT

public:
    explicit AudioWaveformLaneView(int channel, int channelCount,
                                   QGraphicsItem *parent = nullptr);

    void setChannel(int channel, int channelCount);
    [[nodiscard]] int channel() const;

    void setAudioInfo(const AudioInfoModel &info);
    void setTimeline(const Timeline &timeline);
    void setPath(const QString &path);
    void setLoadState(AppGlobal::AudioClipLoadState state);
    // Absolute ticks of the material origin and of the clip's first audible tick,
    // which is also where this view's scene starts (the scene is clip-local, so
    // the sampler needs it to keep converting pixels into global ticks)
    void setMaterialTicks(int materialStartTick, int visibleStartTick);
    void setTrackColorIndex(int index);
    void setCenterLineColor(const QColor &color);
    void invalidateWaveform();

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void updateRectAndPos() override;

private:
    int m_channel = 0;
    int m_channelCount = 1;
    AudioInfoModel m_audioInfo;
    Timeline m_timeline;
    QString m_path;
    AppGlobal::AudioClipLoadState m_loadState = AppGlobal::AudioClipLoadState::Loaded;
    int m_materialStartTick = 0;
    int m_visibleStartTick = 0;
    int m_trackColorIndex = 0;
    QColor m_centerLineColor = {60, 65, 78};
    mutable AudioWaveformSampler m_waveformSampler;
};

#endif // AUDIOWAVEFORMLANEVIEW_H
