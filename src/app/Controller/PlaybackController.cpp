//
// Created by fluty on 2024/1/31.
//

#include "PlaybackController.h"

#include "ValidationController.h"
#include "Model/AppModel.h"
#include "UI/Controls/Toast.h"

PlaybackController::PlaybackController() {
    auto model = AppModel::instance();
    connect(model, &AppModel::tempoChanged, this, &PlaybackController::onTempoChanged);
    connect(ValidationController::instance(), &ValidationController::validationFinished, this,
            &PlaybackController::onValidationFinished);
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
    m_playRequested = true;
    ValidationController::instance()->runValidation();
}

void PlaybackController::pause() {
    m_playRequested = false;
    m_playbackStatus = Paused;
    emit playbackStatusChanged(Paused);
}
void PlaybackController::stop() {
    m_playRequested = false;
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
void PlaybackController::onModelChanged() {
    auto tempo = AppModel::instance()->tempo();
    onTempoChanged(tempo);
}
void PlaybackController::onValidationFinished(bool passed) {
    qDebug() << "PlaybackController::onValidationFinished" << "passed" << passed;
    if (m_playRequested) {
        m_playRequested = false;
        if (passed) {
            m_playbackStatus = Playing;
            emit playbackStatusChanged(Playing);
        } else {
            Toast::show(tr("Please fix project errors before playing"));
        }
    }
}
double PlaybackController::samplePosToTick(int sample) const {
    auto secs = sample / m_sampleRate;
    auto tick = secs * 60 / m_tempo * 480;
    return tick;
}
int PlaybackController::tickToSamplePos(double tick) const {
    auto pos = tick * m_tempo * m_sampleRate / 60 / 480;
    return static_cast<int>(pos);
}