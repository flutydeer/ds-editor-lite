//
// Created by fluty on 2024/1/31.
//

#include "PlaybackController.h"
#include "PlaybackController_p.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include "Model/AppStatus/AppStatus.h"
#include <lite/GUI/Controls/Toast.h>
#include "Global/AppGlobal.h"

PlaybackController::PlaybackController() : d_ptr(new PlaybackControllerPrivate(this)) {
    Q_D(PlaybackController);
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

void PlaybackController::onModelChanged() {
}
