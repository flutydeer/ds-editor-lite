//
// Created by fluty on 2024/2/7.
//

#include "TimeSignatureActions.h"

#include "EditTimeSignatureAction.h"

void TimeSignatureActions::editTimeSignature(TimeSignature oldSig, TimeSignature newSig,
                                             AppModel *model) {
    addAction(EditTimeSignatureAction::build(oldSig, newSig, model));
}