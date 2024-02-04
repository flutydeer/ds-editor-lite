//
// Created by Crs_1 on 2024/2/4.
//

#ifndef DS_EDITOR_LITE_AUDIOCONTEXT_H
#define DS_EDITOR_LITE_AUDIOCONTEXT_H

#include <QObject>
#include <QMap>

#include <TalcsCore/PositionableMixerAudioSource.h>
#include <TalcsCore/SmoothedFloat.h>

namespace talcs {
    class AudioSourceClipSeries;
    class FutureAudioSourceClipSeries;
}

#include "Controller/PlaybackController.h"

class AudioContext : public QObject {
    Q_OBJECT
public:
    explicit AudioContext(QObject *parent = nullptr);

public slots:
    void handlePlaybackStatusChange(PlaybackController::PlaybackStatus status);
    void handlePlaybackPositionChange(double positionTick);

    void handleVstCallbackPositionChange(qint64 positionSample);

    void handleTrackInsertion(DsTrack *track);
    void handleTrackRemoval(DsTrack *track);
    void handleTrackControlChange(DsTrack *track, float gainDb, float pan100x, bool mute, bool solo);

private:
    QMap<DsTrack *, talcs::PositionableMixerAudioSource::SourceIterator> m_trackItDict;
    QMap<DsTrack *, talcs::PositionableMixerAudioSource *> m_trackSourceDict;
    QMap<DsTrack *, talcs::AudioSourceClipSeries *> m_trackAudioClipSeriesDict;
    QMap<DsTrack *, talcs::FutureAudioSourceClipSeries *> m_trackSynthesisClipSeriesDict;
    QMap<DsTrack *, QPair<talcs::SmoothedFloat *, talcs::SmoothedFloat *>> m_trackLevelMeterValue;
};



#endif // DS_EDITOR_LITE_AUDIOCONTEXT_H
