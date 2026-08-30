#include "PlaybackController.h"
#include "PlaybackController_p.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include <lite/ProjectModel/AppModel/AppModel.h>
#include "Model/AppStatus/AppStatus.h"
#include <lite/GUI/Controls/Toast.h>
#include "Global/AppGlobal.h"

#include <QGuiApplication>
#include <QScreen>
#include <QWindow>

#include <chrono>
#include <cmath>
#include <utility>

PlaybackController::PlaybackController() : d_ptr(new PlaybackControllerPrivate(this)) {
    Q_D(PlaybackController);
    d->m_visualPositionTimer.setTimerType(Qt::PreciseTimer);
    connect(&d->m_visualPositionTimer, &QChronoTimer::timeout, this,
            &PlaybackController::requestVisualPositionUpdate);
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
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    if (!runtime)
        return;
    const auto result =
        runtime->playback().play({.expected = runtime->documentVersion(),
                                  .source = Automation::InvocationSource::TrustedGui});
    if (!result && result.getError().code == Automation::AutomationErrorCode::Busy) {
        qWarning() << "Cannot start playing because mouse button not released";
        Toast::show(tr("Please release mouse button before playing"));
    }
}

bool PlaybackController::applyPlay() {
    Q_D(PlaybackController);
    if (d->m_playbackStartGuard && !d->m_playbackStartGuard())
        return false;
    d->m_playbackStatus = Playing;
    d->m_visualPositionAnchor = d->m_position;
    d->m_visualPositionClock.restart();
    emit playbackStatusChanged(Playing);
    updateVisualPositionTimerInterval();
    d->m_visualPositionTimer.start();
    emit visualPositionChanged(d->m_position);
    return true;
}

void PlaybackController::pause() {
    if (auto *runtime = AppContext::instance<Automation::CoreRuntime>()) {
        runtime->playback().pause({.expected = runtime->documentVersion(),
                                   .source = Automation::InvocationSource::TrustedGui});
    }
}

void PlaybackController::applyPause() {
    Q_D(PlaybackController);
    d->m_playbackStatus = Paused;
    d->m_visualPositionTimer.stop();
    emit playbackStatusChanged(Paused);
    emit visualPositionChanged(d->m_position);
}

void PlaybackController::stop() {
    if (auto *runtime = AppContext::instance<Automation::CoreRuntime>()) {
        runtime->playback().stop({.expected = runtime->documentVersion(),
                                  .source = Automation::InvocationSource::TrustedGui});
    }
}

void PlaybackController::applyStop() {
    Q_D(PlaybackController);
    d->m_playbackStatus = Stopped;
    d->m_visualPositionTimer.stop();
    emit playbackStatusChanged(Stopped);
    emit visualPositionChanged(d->m_position);
}

void PlaybackController::setPosition(const double tick) {
    if (auto *runtime = AppContext::instance<Automation::CoreRuntime>()) {
        runtime->playback().setPosition({.expected = runtime->documentVersion(),
                                         .source = Automation::InvocationSource::TrustedGui},
                                        tick);
    }
}

void PlaybackController::applyPosition(const double tick) {
    Q_D(PlaybackController);
    d->m_position = tick;
    d->m_visualPositionAnchor = tick;
    d->m_visualPositionClock.restart();
    emit positionChanged(tick);
    if (d->m_playbackStatus != Playing)
        emit visualPositionChanged(tick);
}

void PlaybackController::setLastPosition(const double tick) {
    if (auto *runtime = AppContext::instance<Automation::CoreRuntime>()) {
        runtime->playback().setLastPosition({.expected = runtime->documentVersion(),
                                             .source = Automation::InvocationSource::TrustedGui},
                                            tick);
    }
}

void PlaybackController::setLoopSettings(const LoopSettings &settings) {
    if (auto *runtime = AppContext::instance<Automation::CoreRuntime>()) {
        runtime->playback().setLoop({.expected = runtime->documentVersion(),
                                     .source = Automation::InvocationSource::TrustedGui},
                                    settings);
    }
}

bool PlaybackController::beginLoopSettingsEdit() {
    Q_D(PlaybackController);
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    if (!runtime || !d->m_loopPreviewHandler)
        return false;
    const auto snapshot = runtime->playback().getPlayback(runtime->documentVersion().documentId);
    if (!snapshot)
        return false;
    d->m_loopEditVersion = runtime->documentVersion();
    d->m_loopEditOriginal = snapshot.get().loop;
    return true;
}

void PlaybackController::previewLoopSettings(const LoopSettings &settings) {
    Q_D(PlaybackController);
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    if (!runtime || !d->m_loopEditVersion ||
        runtime->documentVersion().documentId != d->m_loopEditVersion->documentId) {
        d->m_loopEditVersion.reset();
        return;
    }
    d->m_loopPreviewHandler(settings);
}

void PlaybackController::commitLoopSettingsEdit(const LoopSettings &settings) {
    Q_D(PlaybackController);
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    if (!runtime || !d->m_loopEditVersion ||
        runtime->documentVersion().documentId != d->m_loopEditVersion->documentId) {
        d->m_loopEditVersion.reset();
        return;
    }

    const auto expected = *d->m_loopEditVersion;
    const auto original = d->m_loopEditOriginal;
    d->m_loopEditVersion.reset();
    d->m_loopPreviewHandler(original);
    runtime->playback().setLoop(
        {.expected = expected, .source = Automation::InvocationSource::TrustedGui}, settings);
}

void PlaybackController::setLoopPreviewHandler(std::function<void(const LoopSettings &)> handler) {
    Q_D(PlaybackController);
    d->m_loopPreviewHandler = std::move(handler);
}

void PlaybackController::applyLastPosition(const double tick) {
    Q_D(PlaybackController);
    d->m_lastPlayPosition = tick;
    emit lastPositionChanged(tick);
}

void PlaybackController::requestVisualPositionUpdate() {
    Q_D(PlaybackController);
    if (d->m_playbackStatus != Playing || !d->m_visualPositionClock.isValid())
        return;

    updateVisualPositionTimerInterval();
    const auto &timeline = appModel->timeline();
    const auto anchorMs = timeline.tickToMs(d->m_visualPositionAnchor);
    const auto elapsedMs = d->m_visualPositionClock.nsecsElapsed() / 1000000.0;
    auto visualMs = anchorMs + elapsedMs;
    const auto &loopSettings = appStatus->loopSettings.get();
    if (loopSettings.enabled && loopSettings.length > 0) {
        const auto loopStartMs = timeline.tickToMs(loopSettings.start);
        const auto loopDurationMs = timeline.tickToMs(loopSettings.end()) - loopStartMs;
        if (loopDurationMs > 0.0 && visualMs >= loopStartMs + loopDurationMs)
            visualMs = loopStartMs + std::fmod(visualMs - loopStartMs, loopDurationMs);
    }
    emit visualPositionChanged(timeline.msToTick(visualMs));
}

void PlaybackController::updateVisualPositionTimerInterval() {
    Q_D(PlaybackController);
    const auto *window = QGuiApplication::focusWindow();
    const auto *screen = window ? window->screen() : QGuiApplication::primaryScreen();
    auto refreshRate = screen ? screen->refreshRate() : 60.0;
    if (!std::isfinite(refreshRate) || refreshRate <= 0.0)
        refreshRate = 60.0;
    const auto interval =
        std::chrono::nanoseconds(qMax<qint64>(1, qRound64(1000000000.0 / refreshRate)));
    if (d->m_visualPositionTimer.interval() != interval)
        d->m_visualPositionTimer.setInterval(interval);
}

void PlaybackController::sampleRateChanged(const double sr) {
    Q_D(PlaybackController);
    d->m_sampleRate = sr;
}

void PlaybackController::onModelChanged() {
}
