//
// Created by fluty on 2024/2/10.
//

#include "TimeGraphicsScene.h"
#include "TimeGridGraphicsItem.h"
#include "TimeIndicatorGraphicsItem.h"

void TimeGraphicsScene::addTimeGrid(TimeGridGraphicsItem *item) {
    item->setZValue(-1);
    addCommonItem(item);
}
void TimeGraphicsScene::addTimeIndicator(TimeIndicatorGraphicsItem *item) {
    item->setZValue(100);
    addCommonItem(item);
}