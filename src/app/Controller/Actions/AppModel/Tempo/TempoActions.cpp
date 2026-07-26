//
// Created by fluty on 2024/2/7.
//

#include "TempoActions.h"

#include "EditTempoAction.h"

void TempoActions::editTempo(const double oldTempo, const double newTempo, AppModel *model) {
    // Audio clips are anchored in real time; their tick caches are re-derived
    // inside AppModel::setTempo, so no per-clip re-anchoring action is needed
    addAction(EditTempoAction::build(oldTempo, newTempo, model));
}