#ifndef PIANOROLLGRAPHICSSCENE_H
#define PIANOROLLGRAPHICSSCENE_H

#include "UI/Views/Common/TimeGraphicsScene.h"

class PianoRollGraphicsScene final : public TimeGraphicsScene {
public:
    explicit PianoRollGraphicsScene();

private:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
};

#endif // PIANOROLLGRAPHICSSCENE_H
