//
// Created by fluty on 2024/2/10.
//

#ifndef TIMEGRAPHICSSCENE_H
#define TIMEGRAPHICSSCENE_H

#include "CommonGraphicsScene.h"

class TimeGridGraphicsItem;
class TimeIndicatorGraphicsItem;

class TimeGraphicsScene : public CommonGraphicsScene {
    Q_OBJECT

public:
    explicit TimeGraphicsScene(QObject *parent = nullptr) : CommonGraphicsScene(parent){};

    void addTimeGrid(TimeGridGraphicsItem *item);
    void addTimeIndicator(TimeIndicatorGraphicsItem *item);
    void setPixelsPerQuarterNote(int px);

private:
    friend class TimeGraphicsView;
    void setSceneLength(int tick);
    int m_pixelsPerQuarterNote = 32;
};

#endif // TIMEGRAPHICSSCENE_H
