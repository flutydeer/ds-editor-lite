#ifndef AUDIOVIEWGRAPHICSSCENE_H
#define AUDIOVIEWGRAPHICSSCENE_H

#include "UI/Views/Common/TimeGraphicsScene.h"

// Scene of the audio view. The vertical axis carries no zoom and no scrolling,
// so the scene is exactly one viewport tall and the channel lanes tile it.
class AudioViewGraphicsScene final : public TimeGraphicsScene {
    Q_OBJECT

public:
    AudioViewGraphicsScene();

    void setViewportHeight(double height);
    void setContentLength(int ticks);
};

#endif // AUDIOVIEWGRAPHICSSCENE_H
