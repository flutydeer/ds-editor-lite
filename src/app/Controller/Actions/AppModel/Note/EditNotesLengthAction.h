//
// Created by fluty on 2024/2/8.
//

#ifndef EDITNOTESLENGTHACTION_H
#define EDITNOTESLENGTHACTION_H

#include "Modules/History/IAction.h"

class SingingClip;
class Note;

class EditNotesLengthAction final : public IAction {
public:
    static EditNotesLengthAction *build(Note *note, int deltaTick, SingingClip *clip);
    void execute() override;
    void undo() override;

private:
    Note *m_note = nullptr;
    int m_deltaTick = 0;
    SingingClip *m_clip = nullptr;
};



#endif //EDITNOTESLENGTHACTION_H
