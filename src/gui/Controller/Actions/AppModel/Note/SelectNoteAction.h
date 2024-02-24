//
// Created by fluty on 24-2-16.
//

#ifndef SELECTNOTEACTION_H
#define SELECTNOTEACTION_H

#include "Controller/History/IAction.h"

class SingingClip;
class Note;

class SelectNoteAction final : public IAction {
public:
    static SelectNoteAction *build(Note *note, bool selected, SingingClip *clip);
    void execute() override;
    void undo() override;

private:
    Note *m_note = nullptr;
    SingingClip *m_clip = nullptr;
    bool m_oldValue = false;
    bool m_newValue = false;
};



#endif // SELECTNOTEACTION_H
