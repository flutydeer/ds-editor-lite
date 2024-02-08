//
// Created by fluty on 2024/2/8.
//

#ifndef INSERTNOTEACTION_H
#define INSERTNOTEACTION_H

#include "Controller/History/IAction.h"
#include "Model/DsClip.h"
#include "Model/DsNote.h"

class InsertNoteAction final : public IAction {
public:
    static InsertNoteAction *build(DsNote *note, DsSingingClip *clip);
    void execute() override;
    void undo() override;

private:
    DsNote *m_note = nullptr;
    DsSingingClip *m_clip = nullptr;
};



#endif // INSERTNOTEACTION_H
