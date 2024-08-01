//
// Created by fluty on 2024/2/7.
//

#ifndef EDITTIMESIGNATUREACTION_H
#define EDITTIMESIGNATUREACTION_H

#include "Model/AppModel.h"
#include "Modules/History/IAction.h"

class EditTimeSignatureAction : public IAction {
public:
    static EditTimeSignatureAction *build(TimeSignature oldSig, TimeSignature newSig,
                                          AppModel *model);
    void execute() override;
    void undo() override;

private:
    TimeSignature m_oldSig;
    TimeSignature m_newSig;
    AppModel *m_model = nullptr;
};



#endif // EDITTIMESIGNATUREACTION_H
