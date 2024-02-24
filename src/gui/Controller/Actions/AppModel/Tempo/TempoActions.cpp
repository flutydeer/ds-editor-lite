//
// Created by fluty on 2024/2/7.
//

#include "TempoActions.h"

#include "EditTempoAction.h"

void TempoActions::editTempo(double oldTempo, double newTempo, AppModel *model) {
    addAction(EditTempoAction::build(oldTempo, newTempo, model));
}