//
// Created by fluty on 2024/2/8.
//

#ifndef INSERTNOTEACTION_H
#define INSERTNOTEACTION_H

#include "Controller/History/IAction.h"
#include "Model/Clip.h"
#include "Model/Note.h"

class InsertNoteAction final : public IAction {
public:
    static InsertNoteAction *build(Note *note, DsSingingClip *clip);
    void execute() override;
    void undo() override;

private:
    Note *m_note = nullptr;
    DsSingingClip *m_clip = nullptr;
};



#endif // INSERTNOTEACTION_H
