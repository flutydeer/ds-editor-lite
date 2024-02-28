//
// Created by fluty on 2024/2/8.
//

#ifndef EDITNOTESTARTACTION_H
#define EDITNOTESTARTACTION_H

#include "Modules/History/IAction.h"

class SingingClip;
class Note;

class EditNoteStartAndLengthAction final : public IAction {
public:
    static EditNoteStartAndLengthAction *build(Note *note, int deltaTick, SingingClip *clip);
    void execute() override;
    void undo() override;

private:
    Note *m_note = nullptr;
    int m_deltaTick = 0;
    SingingClip *m_clip = nullptr;
};



#endif //EDITNOTESTARTACTION_H
