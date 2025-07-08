//
// Created by Crs_1 on 2024/2/4.
//

#ifndef AUDIOCONTEXT_H
#define AUDIOCONTEXT_H

#define audioContext AudioContext::instance()

#include <QObject>
#include <QMap>
#include <QTimer>

#include <TalcsCore/PositionableMixerAudioSource.h>
#include <TalcsCore/SmoothedFloat.h>
#include <TalcsCore/AudioSourceClipSeries.h>
#include <TalcsDspx/DspxProjectContext.h>

#include "Controller/PlaybackController.h"
#include "Global/PlaybackGlobal.h"
#include "Model/AppModel/AudioClip.h"
#include "AudioExporter.h"


class TrackInferenceHandler;

namespace talcs {
    class DspxAudioClipContext;
}

class TrackSynthesizer;
class AudioContextAudioExporterListener;

class AudioContext : public talcs::DspxProjectContext, public Audio::AudioExporterListener {
    Q_OBJECT
public:
    explicit AudioContext(QObject *parent = nullptr);
    ~AudioContext() override;

    static AudioContext *instance();

    Track *getTrackFromContext(talcs::DspxTrackContext *trackContext) const;
    AudioClip *getAudioClipFromContext(talcs::DspxAudioClipContext *audioClipContext) const;

    talcs::DspxTrackContext *getContextFromTrack(Track *trackModel) const;
    talcs::DspxAudioClipContext *getContextFromAudioClip(AudioClip *audioClipModel) const;

    void handlePanSliderMoved(Track *track, double pan) const;
    void handleGainSliderMoved(Track *track, double gain) const;
    void handleMasterPanSliderMoved(double pan) const;
    void handleMasterGainSliderMoved(double gain) const;

    void handleInferPieceFailed() const;

signals:
    void levelMeterUpdated(const AppModel::LevelMetersUpdatedArgs &args);
    void exporterCausedTimeChanged();

private:
    enum PlayheadBehavior {
        ReturnToStart,
        KeepAtCurrent,
        KeepAtCurrentButPlayFromStart,
    };

    PlaybackStatus m_lastStatus = PlaybackGlobal::Stopped;

    QHash<Track *, talcs::DspxTrackContext *> m_trackModelDict;
    QHash<AudioClip *, talcs::DspxAudioClipContext *> m_audioClipModelDict;

    QHash<Track *, TrackSynthesizer *> m_trackSynthDict;
    QHash<Track *, TrackInferenceHandler *> m_trackInferDict;

    QTimer *m_levelMeterTimer;
    QHash<const Track *,
          QPair<std::shared_ptr<talcs::SmoothedFloat>, std::shared_ptr<talcs::SmoothedFloat>>>
        m_trackLevelMeterValue;
    Track *masterChannel;

    int m_levelMeterRampLength = 128;

    bool m_transportPositionFlag = true;

    void handlePlaybackStatusChanged(PlaybackStatus status);
    void handlePlaybackPositionChanged(double positionTick);

    void handleModelChanged();
    void handleTrackInserted(int index, Track *track);
    void handleTrackRemoved(int index, Track *track);

    void handleMasterControlChanged(const TrackControl &control);
    void handleTrackControlChanged(Track *track);
    void handleClipInserted(Track *track, int id, AudioClip *audioClip);
    void handleClipRemoved(Track *track, int id, AudioClip *audioClip);

    void handleClipPropertyChanged(AudioClip *audioClip) const;

    void handleTimeChanged();

    bool willStartCallback(AudioExporter *exporter) override;
    void willFinishCallback(AudioExporter *exporter) override;

    void updateTrackLevelMeterValue(Track *track, QList<float> values);
};

#endif // AUDIOCONTEXT_H
