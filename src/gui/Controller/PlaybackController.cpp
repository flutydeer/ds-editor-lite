//
// Created by fluty on 2024/1/31.
//

#include "PlaybackController.h"


#include "Model/AppModel.h"

PlaybackController::PlaybackController() {
    auto model = AppModel::instance();
    connect(model, &AppModel::tempoChanged, this, &PlaybackController::onTempoChanged);
}
void PlaybackController::play() {
    // TODO: call audio backend
    m_lastPlayPosition = m_position;
    emit lastPositionChanged(samplePosToTick(m_lastPlayPosition));
}

void PlaybackController::setPosition(double tick) {
    emit positionChanged(tick); // TODO: remove
}
void PlaybackController::sampleRateChanged(int sr) {
    m_sampleRate = sr;
}
void PlaybackController::onTempoChanged(double tempo) {
    m_tempo = tempo;
}
double PlaybackController::samplePosToTick(int sample) const {
    auto secs = static_cast<double>(sample) / m_sampleRate;
    auto tick =  secs * 60 / m_tempo * 480;
    return tick;
}
int PlaybackController::tickToSamplePos(double tick) const {
    auto pos = tick * m_tempo * m_sampleRate / 60 / 480;
    return static_cast<int>(pos);
}