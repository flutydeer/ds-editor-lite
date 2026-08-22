#ifndef PLAYBACKCONTROLLER_H
#define PLAYBACKCONTROLLER_H

#define playbackController PlaybackController::instance()

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/Core/Singleton.h>
#include "Global/PlaybackGlobal.h"

#include <QObject>

#include <functional>

using namespace PlaybackGlobal;

class PlaybackControllerPrivate;
class AppContext;

class PlaybackController final : public QObject {
    Q_OBJECT

private:
    explicit PlaybackController();
    ~PlaybackController() override;

public:
    LITE_SINGLETON_DECLARE_INSTANCE(PlaybackController)
    Q_DISABLE_COPY_MOVE(PlaybackController)

public:
    [[nodiscard]] PlaybackStatus playbackStatus() const;

    [[nodiscard]] double position() const;
    [[nodiscard]] double lastPosition() const;
    void setPlaybackStartGuard(std::function<bool()> guard);

    signals:
    void positionChanged(double tick);
    void visualPositionChanged(double tick);
    void lastPositionChanged(double tick);
    void playbackStatusChanged(PlaybackStatus status);

public slots:
    void play();
    void pause();
    void stop();

    void setPosition(double tick);
    void setLastPosition(double tick);

    void sampleRateChanged(double sr);
    void onModelChanged();

private:
    friend class AppContext;

    bool applyPlay();
    void applyPause();
    void applyStop();
    void applyPosition(double tick);
    void applyLastPosition(double tick);
    void requestVisualPositionUpdate();
    void updateVisualPositionTimerInterval();

    Q_DECLARE_PRIVATE(PlaybackController)
    PlaybackControllerPrivate *d_ptr;
};



#endif // PLAYBACKCONTROLLER_H
