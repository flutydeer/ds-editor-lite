//
// Created by fluty on 2024/2/8.
//

#ifndef EDITNOTESWORDPROPERTIESACTION_H
#define EDITNOTESWORDPROPERTIESACTION_H

#include "Controller/History/IAction.h"
#include "Model/Clip.h"

class EditNotesWordPropertiesAction final : public IAction {
public:
    static EditNotesWordPropertiesAction *build(Note *note, Note::NoteWordProperties *args, DsSingingClip *clip);
    void execute() override;
    void undo() override;

private:
    Note *m_note = nullptr;
    DsSingingClip *m_clip = nullptr;
    Note::NoteWordProperties m_oldArgs;
    Note::NoteWordProperties m_newArgs;
};



#endif //EDITNOTESWORDPROPERTIESACTION_H
