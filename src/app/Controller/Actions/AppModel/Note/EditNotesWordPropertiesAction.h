//
// Created by fluty on 2024/2/8.
//

#ifndef EDITNOTESWORDPROPERTIESACTION_H
#define EDITNOTESWORDPROPERTIESACTION_H

#include "Model/AppModel/Note.h"
#include "Modules/History/IAction.h"

class SingingClip;

class EditNotesWordPropertiesAction final : public IAction {
public:
    static EditNotesWordPropertiesAction *build(Note *note, Note::NoteWordProperties args);
    void execute() override;
    void undo() override;

private:
    Note *m_note = nullptr;
    Note::NoteWordProperties m_oldArgs;
    Note::NoteWordProperties m_newArgs;
};



#endif //EDITNOTESWORDPROPERTIESACTION_H
