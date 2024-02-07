//
// Created by fluty on 2024/2/7.
//

#ifndef EDITTEMPOACTION_H
#define EDITTEMPOACTION_H

#include "Controller/History/IAction.h"
#include "Model/AppModel.h"

class EditTempoAction final : public IAction {
public:
    static EditTempoAction *build(double oldTempo, double newTempo, AppModel *model);
    void execute() override;
    void undo() override;

private:
    double m_oldTempo = 0;
    double m_newTempo = 0;
    AppModel *m_model = nullptr;
};



#endif // EDITTEMPOACTION_H
