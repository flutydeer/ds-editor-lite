//
// Created by fluty on 2024/2/8.
//

#ifndef EDITNOTESTARTACTION_H
#define EDITNOTESTARTACTION_H

#include "Controller/History/IAction.h"
#include "Model/DsClip.h"

class EditNoteStartAndLengthAction final : public IAction {
public:
    static EditNoteStartAndLengthAction *build(DsNote *note, int deltaTick, DsSingingClip *clip);
    void execute() override;
    void undo() override;

private:
    DsNote *m_note = nullptr;
    int m_deltaTick = 0;
    DsSingingClip *m_clip = nullptr;
};



#endif //EDITNOTESTARTACTION_H
