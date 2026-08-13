#include "PlaybackController.h"
#include "PlaybackController_p.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include "Model/AppStatus/AppStatus.h"
#include <lite/GUI/Controls/Toast.h>
#include "Global/AppGlobal.h"

PlaybackController::PlaybackController() : d_ptr(new PlaybackControllerPrivate(this)) {
    Q_D(PlaybackController);
    d->m_visualPositionTimer.setInterval(positionUpdateIntervalMs);
    d->m_visualPositionTimer.setTimerType(Qt::PreciseTimer);
    connect(&d->m_visualPositionTimer, &QTimer::timeout, this,
            &PlaybackController::updateVisualPosition);
    connect(appModel, &AppModel::timelineChanged, this, [d, this] {
        d->m_visualPositionAnchor = d->m_position;
        d->m_visualPositionClock.restart();
        if (d->m_playbackStatus == Playing)
            emit visualPositionChanged(d->m_position);
    });
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

void PlaybackController::setPlaybackStartGuard(std::function<bool()> guard) {
    Q_D(PlaybackController);
    d->m_playbackStartGuard = std::move(guard);
}

void PlaybackController::play() {
    Q_D(PlaybackController);
    if (appStatus->currentEditObject != AppStatus::EditObjectType::None) {
        qWarning() << "Cannot start playing because mouse button not released";
        Toast::show(tr("Please release mouse button before playing"));
        return;
    }
    if (d->m_playbackStartGuard && !d->m_playbackStartGuard())
        return;
    d->m_playbackStatus = Playing;
    d->m_visualPositionAnchor = d->m_position;
    d->m_visualPositionClock.restart();
    emit playbackStatusChanged(Playing);
    d->m_visualPositionTimer.start();
    emit visualPositionChanged(d->m_position);
}

void PlaybackController::pause() {
    Q_D(PlaybackController);
    d->m_playbackStatus = Paused;
    d->m_visualPositionTimer.stop();
    emit playbackStatusChanged(Paused);
    emit visualPositionChanged(d->m_position);
}

void PlaybackController::stop() {
    Q_D(PlaybackController);
    d->m_playbackStatus = Stopped;
    d->m_visualPositionTimer.stop();
    emit playbackStatusChanged(Stopped);
    emit visualPositionChanged(d->m_position);
}

void PlaybackController::setPosition(const double tick) {
    Q_D(PlaybackController);
    d->m_position = tick;
    d->m_visualPositionAnchor = tick;
    d->m_visualPositionClock.restart();
    emit positionChanged(tick);
    if (d->m_playbackStatus != Playing)
        emit visualPositionChanged(tick);
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

void PlaybackController::updateVisualPosition() {
    Q_D(PlaybackController);
    if (d->m_playbackStatus != Playing || !d->m_visualPositionClock.isValid())
        return;

    const auto &timeline = appModel->timeline();
    const auto anchorMs = timeline.tickToMs(d->m_visualPositionAnchor);
    emit visualPositionChanged(timeline.msToTick(anchorMs + d->m_visualPositionClock.elapsed()));
}
