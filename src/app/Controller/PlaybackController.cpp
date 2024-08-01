//
// Created by fluty on 2024/1/31.
//

#include "PlaybackController.h"
#include "PlaybackController_p.h"

#include "ValidationController.h"
#include "Model/AppModel.h"
#include "UI/Controls/Toast.h"

PlaybackController::PlaybackController() : d_ptr(new PlaybackControllerPrivate(this)) {
    Q_D(PlaybackController);
    connect(appModel, &AppModel::tempoChanged, this, &PlaybackController::onTempoChanged);
    connect(ValidationController::instance(), &ValidationController::validationFinished, this,
            [=](bool passed) { d->onValidationFinished(passed); });
}
PlaybackController::~PlaybackController() {
    delete d_ptr;
}

PlaybackStatus PlaybackController::playbackStatus() const {
    Q_D(const PlaybackController);
    return d->m_playbackStatus;
}
double PlaybackController::position() const {
    Q_D(const PlaybackController);
    return d->m_position;
}
double PlaybackController::lastPosition() const {
    Q_D(const PlaybackController);
    return d->m_lastPlayPosition;
}
double PlaybackController::tempo() const {
    Q_D(const PlaybackController);
    return d->m_tempo;
}

void PlaybackController::play() {
    Q_D(PlaybackController);
    d->m_playRequested = true;
    ValidationController::instance()->runValidation();
}

void PlaybackController::pause() {
    Q_D(PlaybackController);
    d->m_playRequested = false;
    d->m_playbackStatus = Paused;
    emit playbackStatusChanged(Paused);
}
void PlaybackController::stop() {
    Q_D(PlaybackController);
    d->m_playRequested = false;
    d->m_playbackStatus = Stopped;
    emit playbackStatusChanged(Stopped);
}

void PlaybackController::setPosition(double tick) {
    Q_D(PlaybackController);
    d->m_position = tick;
    emit positionChanged(tick);
}
void PlaybackController::setLastPosition(double tick) {
    Q_D(PlaybackController);
    d->m_lastPlayPosition = tick;
    emit lastPositionChanged(tick);
}

void PlaybackController::sampleRateChanged(double sr) {
    Q_D(PlaybackController);
    d->m_sampleRate = sr;
}
void PlaybackController::onTempoChanged(double tempo) {
    Q_D(PlaybackController);
    d->m_tempo = tempo;
}
void PlaybackController::onModelChanged() {
    auto tempo = appModel->tempo();
    onTempoChanged(tempo);
}
void PlaybackControllerPrivate::onValidationFinished(bool passed) {
    Q_Q(PlaybackController);
    qDebug() << "PlaybackController::onValidationFinished"
             << "passed" << passed;
    if (m_playRequested) {
        m_playRequested = false;
        if (passed) {
            m_playbackStatus = Playing;
            emit q->playbackStatusChanged(Playing);
        } else {
            Toast::show(tr("Please fix project errors before playing"));
        }
    }
}
double PlaybackControllerPrivate::samplePosToTick(int sample) const {
    auto secs = sample / m_sampleRate;
    auto tick = secs * 60 / m_tempo * 480;
    return tick;
}
int PlaybackControllerPrivate::tickToSamplePos(double tick) const {
    auto pos = tick * m_tempo * m_sampleRate / 60 / 480;
    return static_cast<int>(pos);
}