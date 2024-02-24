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
    void addTimeGrid(TimeGridGraphicsItem *item);
    void addTimeIndicator(TimeIndicatorGraphicsItem *item);

};

#endif // TIMEGRAPHICSSCENE_H
