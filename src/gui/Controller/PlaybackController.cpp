//
// Created by fluty on 2024/1/31.
//

#include "PlaybackController.h"


#include "Model/AppModel.h"

PlaybackController::PlaybackController() {
    auto model = AppModel::instance();
    connect(model, &AppModel::tempoChanged, this, &PlaybackController::onTempoChanged);
}

PlaybackController::PlaybackStatus PlaybackController::playbackStatus() const {
    return m_playbackStatus;
}
double PlaybackController::position() const {
    return m_position;
}
double PlaybackController::lastPosition() const {
    return m_lastPlayPosition;
}
double PlaybackController::tempo() const {
    return m_tempo;
}

void PlaybackController::play() {
    m_playbackStatus = Playing;
    emit playbackStatusChanged(Playing);

    // test level meter
    AppModel::LevelMetersUpdatedArgs args;
    AppModel::LevelMetersUpdatedArgs::State trackState1;
    trackState1.valueL = 0.92;
    trackState1.valueR = 0.8;
    trackState1.clippedL = true;
    trackState1.clippedR = false;
    args.trackMeterStates.append(trackState1);
    emit levelMetersUpdated(args);
}

void PlaybackController::pause() {
    m_playbackStatus = Paused;
    emit playbackStatusChanged(Paused);
}
void PlaybackController::stop() {
    m_playbackStatus = Stopped;
    emit playbackStatusChanged(Stopped);
}

void PlaybackController::setPosition(double tick) {
    m_position = tick;
    emit positionChanged(tick);
}
void PlaybackController::setLastPosition(double tick) {
    m_lastPlayPosition = tick;
    emit lastPositionChanged(tick);
}

void PlaybackController::sampleRateChanged(double sr) {
    m_sampleRate = sr;
}
void PlaybackController::onTempoChanged(double tempo) {
    m_tempo = tempo;
}
double PlaybackController::samplePosToTick(int sample) const {
    auto secs = sample / m_sampleRate;
    auto tick =  secs * 60 / m_tempo * 480;
    return tick;
}
int PlaybackController::tickToSamplePos(double tick) const {
    auto pos = tick * m_tempo * m_sampleRate / 60 / 480;
    return static_cast<int>(pos);
}