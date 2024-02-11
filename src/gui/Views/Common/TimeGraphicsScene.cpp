//
// Created by fluty on 2024/2/10.
//

#include "TimeGraphicsScene.h"

void TimeGraphicsScene::addTimeGrid(TimeGridGraphicsItem *item) {
    item->setZValue(-1);
    addItem(item);
}
void TimeGraphicsScene::addTimeIndicator(TimeIndicatorGraphicsItem *item) {
    item->setZValue(1);
    addItem(item);
}