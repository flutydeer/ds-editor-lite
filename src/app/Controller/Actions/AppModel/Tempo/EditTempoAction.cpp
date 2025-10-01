//
// Created by fluty on 2024/2/7.
//

#include "EditTempoAction.h"

#include "Model/AppModel/AppModel.h"

EditTempoAction *EditTempoAction::build(const double oldTempo, const double newTempo, AppModel *model) {
    const auto a = new EditTempoAction;
    a->m_oldTempo = oldTempo;
    a->m_newTempo = newTempo;
    a->m_model = model;
    return a;
}

void EditTempoAction::execute() {
    m_model->setTempo(m_newTempo);
}

void EditTempoAction::undo() {
    m_model->setTempo(m_oldTempo);
}