//
// Created by Crs_1 on 2024/2/4.
//

#ifndef AUDIOCONTEXT_H
#define AUDIOCONTEXT_H

#define audioContext AudioContext::instance()

#include <QElapsedTimer>
#include <QTimer>

#include <TalcsCore/PositionableMixerAudioSource.h>
#include <TalcsCore/SmoothedFloat.h>
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

    Track *getTrackFromContext(const talcs::DspxTrackContext *trackContext) const;
    AudioClip *getAudioClipFromContext(const talcs::DspxAudioClipContext *audioClipContext) const;

    talcs::DspxTrackContext *getContextFromTrack(Track *trackModel) const;
    talcs::DspxAudioClipContext *getContextFromAudioClip(AudioClip *audioClipModel) const;

    void handlePanSliderMoved(Track *track, double pan) const;
    void handleGainSliderMoved(Track *track, double gain) const;
    void handleMasterPanSliderMoved(double pan) const;
    void handleMasterGainSliderMoved(double gain) const;

    static void handleInferPieceFailed();

signals:
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

    QTimer *m_levelMeterTimer = nullptr;
    bool m_levelMeterActive = false;
    QElapsedTimer m_levelMeterTickTime;
    QHash<const Track *,
          QPair<std::shared_ptr<talcs::SmoothedFloat>, std::shared_ptr<talcs::SmoothedFloat>>>
        m_trackLevelMeterValue;
    std::shared_ptr<talcs::SmoothedFloat> m_masterLevelMeterValueL;
    std::shared_ptr<talcs::SmoothedFloat> m_masterLevelMeterValueR;

    int m_levelMeterRampLength = 128;

    bool m_transportPositionFlag = true;

    void handlePlaybackStatusChanged(PlaybackStatus status);
    void handlePlaybackPositionChanged(double positionTick) const;
    void tickLevelMeters();

    void handleModelChanged();
    void handleTrackInserted(int index, Track *track);
    void handleTrackRemoved(int index, Track *track);

    void handleMasterControlChanged(const TrackControl &control) const;
    void handleTrackControlChanged(Track *track) const;
    void handleClipInserted(Track *track, int id, AudioClip *audioClip);
    void handleClipRemoved(Track *track, int id, AudioClip *audioClip);

    void handleClipPropertyChanged(AudioClip *audioClip) const;

    void handleTimeChanged() const;

    bool willStartCallback(AudioExporter *exporter) override;
    void willFinishCallback(AudioExporter *exporter) override;

    static void updateSmoothedValue(std::shared_ptr<talcs::SmoothedFloat> &sm, float dBL);
};

#endif // AUDIOCONTEXT_H
