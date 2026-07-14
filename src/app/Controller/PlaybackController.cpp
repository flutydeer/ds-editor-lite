//
// Created by fluty on 2024/1/31.
//

#include "PlaybackController.h"
#include "PlaybackController_p.h"

#include "Model/AppModel/AppModel.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Controls/Toast.h"
#include "Global/AppGlobal.h"

PlaybackController::PlaybackController() : d_ptr(new PlaybackControllerPrivate(this)) {
    Q_D(PlaybackController);
    connect(appModel, &AppModel::tempoChanged, this, &PlaybackController::onTempoChanged);
    connect(appModel, &AppModel::modelChanged, this, [d, this] {
        if (d->m_playbackStatus != Stopped) {
            stop();
            setPosition(0);
            setLastPosition(0);
        }
    });
}

PlaybackController::~PlaybackController() {
    delete d_ptr;
}

LITE_SINGLETON_IMPLEMENT_INSTANCE(PlaybackController)

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
    if (appStatus->currentEditObject != AppStatus::EditObjectType::None) {
        qWarning() << "Cannot start playing because mouse button not released";
        Toast::show(tr("Please release mouse button before playing"));
        return;
    }
    d->m_playbackStatus = Playing;
    emit playbackStatusChanged(Playing);
}

void PlaybackController::pause() {
    Q_D(PlaybackController);
    d->m_playbackStatus = Paused;
    emit playbackStatusChanged(Paused);
}

void PlaybackController::stop() {
    Q_D(PlaybackController);
    d->m_playbackStatus = Stopped;
    emit playbackStatusChanged(Stopped);
}

void PlaybackController::setPosition(const double tick) {
    Q_D(PlaybackController);
    d->m_position = tick;
    emit positionChanged(tick);
}

void PlaybackController::setLastPosition(const double tick) {
    Q_D(PlaybackController);
    d->m_lastPlayPosition = tick;
    emit lastPositionChanged(tick);
}

void PlaybackController::sampleRateChanged(const double sr) {
    Q_D(PlaybackController);
    d->m_sampleRate = sr;
}

void PlaybackController::onTempoChanged(const double tempo) {
    Q_D(PlaybackController);
    d->m_tempo = tempo;
}

void PlaybackController::onModelChanged() {
    const auto tempo = appModel->tempo();
    onTempoChanged(tempo);
}

double PlaybackControllerPrivate::samplePosToTick(const int sample) const {
    const auto secs = sample / m_sampleRate;
    const auto tick = secs * 60 / m_tempo * AppGlobal::ticksPerQuarterNote;
    return tick;
}

int PlaybackControllerPrivate::tickToSamplePos(const double tick) const {
    const auto pos = tick * m_tempo * m_sampleRate / 60 / AppGlobal::ticksPerQuarterNote;
    return static_cast<int>(pos);
}
