//
// Created by fluty on 2024/2/8.
//

#ifndef INSERTNOTEACTION_H
#define INSERTNOTEACTION_H

#include "Controller/History/IAction.h"

class SingingClip;
class Note;

class InsertNoteAction final : public IAction {
public:
    static InsertNoteAction *build(Note *note, SingingClip *clip);
    void execute() override;
    void undo() override;

private:
    Note *m_note = nullptr;
    SingingClip *m_clip = nullptr;
};



#endif // INSERTNOTEACTION_H
