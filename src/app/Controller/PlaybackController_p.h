#ifndef PLAYBACKCONTROLLER_P_H
#define PLAYBACKCONTROLLER_P_H

#include "Automation/AutomationTypes.h"

#include <QChronoTimer>
#include <QElapsedTimer>

#include <functional>
#include <optional>

class PlaybackControllerPrivate : public QObject {
    Q_OBJECT
    Q_DECLARE_PUBLIC(PlaybackController)

public:
    explicit PlaybackControllerPrivate(PlaybackController *q) : q_ptr(q) {
    }

    double m_position = 0;
    double m_lastPlayPosition = 0;
    double m_sampleRate = 48000;
    PlaybackStatus m_playbackStatus = Stopped;
    double m_visualPositionAnchor = 0;
    QElapsedTimer m_visualPositionClock;
    QChronoTimer m_visualPositionTimer;
    std::function<bool()> m_playbackStartGuard;
    std::function<void(const LoopSettings &)> m_loopPreviewHandler;
    std::optional<Automation::DocumentVersion> m_loopEditVersion;
    LoopSettings m_loopEditOriginal;

private:
    PlaybackController *q_ptr;
};

#endif // PLAYBACKCONTROLLER_P_H
