//
// Created by fluty on 2024/2/7.
//

#include "EditTimeSignatureAction.h"

EditTimeSignatureAction *EditTimeSignatureAction::build(TimeSignature oldSig, TimeSignature newSig,
                                                        AppModel *model) {
    auto a = new EditTimeSignatureAction;
    a->m_oldSig = oldSig;
    a->m_newSig = newSig;
    a->m_model = model;
    return a;
}

void EditTimeSignatureAction::execute() {
    m_model->setTimeSignature(m_newSig);
}

void EditTimeSignatureAction::undo() {
    m_model->setTimeSignature(m_oldSig);
}