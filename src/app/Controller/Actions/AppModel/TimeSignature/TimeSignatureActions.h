//
// Created by fluty on 2024/2/7.
//

#ifndef TIMESIGNATUREACTIONS_H
#define TIMESIGNATUREACTIONS_H

#include "Model/AppModel.h"
#include "Modules/History/ActionSequence.h"

class TimeSignatureActions : public ActionSequence {
public:
    void editTimeSignature(AppModel::TimeSignature oldSig, AppModel::TimeSignature newSig,
                           AppModel *model);
};



#endif // TIMESIGNATUREACTIONS_H
