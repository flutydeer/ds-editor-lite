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
#include "Global/PlaybackGlobal.h"

class QFile;

namespace talcs {
    class BufferingAudioSource;
}

class AudioContext : public QObject {
    Q_OBJECT
public:
    class SynthesisListener {
    public:
        virtual void trackInsertedCallback(const Track *track, talcs::FutureAudioSourceClipSeries *clipSeries) = 0;
        virtual void trackWillRemoveCallback(const Track *track, talcs::FutureAudioSourceClipSeries *clipSeries) = 0;
        virtual void clipRebuildCallback() = 0;
        virtual void fileBufferingSizeChangeCallback() = 0;
    };

    explicit AudioContext(QObject *parent = nullptr);
    ~AudioContext() override;

    talcs::FutureAudioSourceClipSeries *trackSynthesisClipSeries(const Track *track);
    void addSynthesisListener(SynthesisListener *listener);
    void removeSynthesisListener(SynthesisListener *listener);

public slots:
    void handlePlaybackStatusChange(PlaybackStatus status);
    void handlePlaybackPositionChange(double positionTick);

    void handleVstCallbackPositionChange(qint64 positionSample);

    void handleModelChange();

    void handleTrackInsertion(const Track *track);
    void handleTrackRemoval(const Track *track);
    void handleTrackControlChange(const Track *track);

    void handleClipInsertion(const Track *track, const Clip *clip);
    void handleClipRemoval(const Track *track, const Clip *clip);
    void handleClipPropertyChange(const Track *track, const Clip *clip);

    void rebuildAllClips();

    void handleFileBufferingSizeChange();

    void handleDeviceChangeDuringPlayback();

signals:
    void levelMeterUpdated(const AppModel::LevelMetersUpdatedArgs &args);

private:
    QMap<const Track *, talcs::PositionableMixerAudioSource::SourceIterator> m_trackItDict;
    QMap<const Track *, talcs::PositionableMixerAudioSource *> m_trackSourceDict; // managed
    QMap<const Track *, talcs::AudioSourceClipSeries *> m_trackAudioClipSeriesDict;
    QMap<const Track *, talcs::FutureAudioSourceClipSeries *> m_trackSynthesisClipSeriesDict;
    QMap<const Track *, QPair<std::shared_ptr<talcs::SmoothedFloat>, std::shared_ptr<talcs::SmoothedFloat>>> m_trackLevelMeterValue;

    QMap<const Clip *, talcs::AudioSourceClipSeries::ClipView> m_audioClips;
    QMap<const Clip *, talcs::PositionableMixerAudioSource *> m_audioClipMixers; // managed
    QMap<const Clip *, std::shared_ptr<QFile>> m_audioFiles;
    QMap<const Clip *, talcs::BufferingAudioSource *> m_audioClipBufferingSources;

    QTimer *m_levelMeterTimer;

    QList<SynthesisListener *> m_synthesisListeners;
};



#endif // DS_EDITOR_LITE_AUDIOCONTEXT_H
