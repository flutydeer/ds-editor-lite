//
// Created by Crs_1 on 2024/2/4.
//

#ifndef DS_EDITOR_LITE_AUDIOCONTEXT_H
#define DS_EDITOR_LITE_AUDIOCONTEXT_H

#include <QObject>
#include <QMap>
#include <QTimer>

#include <TalcsCore/PositionableMixerAudioSource.h>
#include <TalcsCore/SmoothedFloat.h>
#include <TalcsCore/AudioSourceClipSeries.h>
#include <TalcsSynthesis/FutureAudioSourceClipSeries.h>

#include "Controller/PlaybackController.h"

class QFile;

class AudioContext : public QObject {
    Q_OBJECT
public:
    explicit AudioContext(QObject *parent = nullptr);
    ~AudioContext() override;

public slots:
    void handlePlaybackStatusChange(PlaybackController::PlaybackStatus status);
    void handlePlaybackPositionChange(double positionTick);

    void handleVstCallbackPositionChange(qint64 positionSample);

    void handleModelChange();

    void handleTrackInsertion(DsTrack *track);
    void handleTrackRemoval(DsTrack *track);
    void handleTrackControlChange(DsTrack *track, float gainDb, float pan100x, bool mute, bool solo);

    void handleClipInsertion(DsTrack *track, DsClip *clip);
    void handleClipRemoval(DsTrack *track, DsClip *clip);
    void handleClipPropertyChange(DsTrack *track, DsClip *clip);

signals:
    void levelMeterUpdated(const AppModel::LevelMetersUpdatedArgs &args);

private:
    QMap<DsTrack *, talcs::PositionableMixerAudioSource::SourceIterator> m_trackItDict;
    QMap<DsTrack *, talcs::PositionableMixerAudioSource *> m_trackSourceDict; // managed
    QMap<DsTrack *, talcs::AudioSourceClipSeries *> m_trackAudioClipSeriesDict;
    QMap<DsTrack *, talcs::FutureAudioSourceClipSeries *> m_trackSynthesisClipSeriesDict;
    QMap<DsTrack *, QPair<std::shared_ptr<talcs::SmoothedFloat>, std::shared_ptr<talcs::SmoothedFloat>>> m_trackLevelMeterValue;

    QMap<DsClip *, talcs::AudioSourceClipSeries::ClipView> m_audioClips;
    QMap<DsClip *, talcs::PositionableMixerAudioSource *> m_audioClipMixers; // managed
    QMap<DsClip *, std::shared_ptr<QFile>> m_audioFiles;

    QTimer *m_levelMeterTimer;
};



#endif // DS_EDITOR_LITE_AUDIOCONTEXT_H
