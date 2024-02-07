//
// Created by fluty on 2024/2/7.
//

#include "TimeSignatureActions.h"


#include "EditTimeSignatureAction.h"
void TimeSignatureActions::editTimeSignature(AppModel::TimeSignature oldSig,
                                             AppModel::TimeSignature newSig, AppModel *model) {
    auto a = EditTimeSignatureAction::build(oldSig, newSig, model);
    m_actionSequence.append(a);
}