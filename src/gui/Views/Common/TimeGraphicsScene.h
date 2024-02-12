//
// Created by fluty on 2024/2/10.
//

#ifndef TIMEGRAPHICSSCENE_H
#define TIMEGRAPHICSSCENE_H

#include "CommonGraphicsScene.h"
#include "TimeGridGraphicsItem.h"
#include "TimeIndicatorGraphicsItem.h"

class TimeGraphicsScene : public CommonGraphicsScene {
    Q_OBJECT

public:
    void addTimeGrid(TimeGridGraphicsItem *item);
    void addTimeIndicator(TimeIndicatorGraphicsItem *item);

};

#endif // TIMEGRAPHICSSCENE_H
