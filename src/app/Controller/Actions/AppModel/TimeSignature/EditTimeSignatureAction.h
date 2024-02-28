//
// Created by fluty on 2024/2/7.
//

#ifndef EDITTIMESIGNATUREACTION_H
#define EDITTIMESIGNATUREACTION_H

#include "Modules/History/IAction.h"
#include "Model/AppModel.h"

class EditTimeSignatureAction : public IAction {
public:
    static EditTimeSignatureAction *build(AppModel::TimeSignature oldSig,
                                          AppModel::TimeSignature newSig, AppModel *model);
    void execute() override;
    void undo() override;

private:
    AppModel::TimeSignature m_oldSig;
    AppModel::TimeSignature m_newSig;
    AppModel *m_model = nullptr;
};



#endif // EDITTIMESIGNATUREACTION_H
