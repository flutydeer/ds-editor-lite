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

namespace talcs {
    class BufferingAudioSource;
}

class AudioContext : public QObject {
    Q_OBJECT
public:
    explicit AudioContext(QObject *parent = nullptr);
    ~AudioContext() override;

    talcs::FutureAudioSourceClipSeries *trackSynthesisClipSeries(const DsTrack *track);

public slots:
    void handlePlaybackStatusChange(PlaybackController::PlaybackStatus status);
    void handlePlaybackPositionChange(double positionTick);

    void handleVstCallbackPositionChange(qint64 positionSample);

    void handleModelChange();

    void handleTrackInsertion(const DsTrack *track);
    void handleTrackRemoval(const DsTrack *track);
    void handleTrackControlChange(const DsTrack *track);

    void handleClipInsertion(const DsTrack *track, const DsClip *clip);
    void handleClipRemoval(const DsTrack *track, const DsClip *clip);
    void handleClipPropertyChange(const DsTrack *track, const DsClip *clip);

    void rebuildAllClips();

    void handleFileBufferingSizeChange();

signals:
    void levelMeterUpdated(const AppModel::LevelMetersUpdatedArgs &args);

private:
    QMap<const DsTrack *, talcs::PositionableMixerAudioSource::SourceIterator> m_trackItDict;
    QMap<const DsTrack *, talcs::PositionableMixerAudioSource *> m_trackSourceDict; // managed
    QMap<const DsTrack *, talcs::AudioSourceClipSeries *> m_trackAudioClipSeriesDict;
    QMap<const DsTrack *, talcs::FutureAudioSourceClipSeries *> m_trackSynthesisClipSeriesDict;
    QMap<const DsTrack *, QPair<std::shared_ptr<talcs::SmoothedFloat>, std::shared_ptr<talcs::SmoothedFloat>>> m_trackLevelMeterValue;

    QMap<const DsClip *, talcs::AudioSourceClipSeries::ClipView> m_audioClips;
    QMap<const DsClip *, talcs::PositionableMixerAudioSource *> m_audioClipMixers; // managed
    QMap<const DsClip *, std::shared_ptr<QFile>> m_audioFiles;
    QMap<const DsClip *, talcs::BufferingAudioSource *> m_audioClipBufferingSources;

    QTimer *m_levelMeterTimer;
};



#endif // DS_EDITOR_LITE_AUDIOCONTEXT_H
