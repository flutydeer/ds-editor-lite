//
// Created by fluty on 2024/2/8.
//

#ifndef EDITNOTESWORDPROPERTIESACTION_H
#define EDITNOTESWORDPROPERTIESACTION_H

#include "Model/AppModel/Note.h"
#include "Modules/History/IAction.h"

class SingingClip;

class EditNoteWordPropertiesAction final : public IAction {
public:
    static EditNoteWordPropertiesAction *build(Note *note, Note::WordProperties args);
    void execute() override;
    void undo() override;

private:
    Note *m_note = nullptr;
    Note::WordProperties m_oldArgs;
    Note::WordProperties m_newArgs;
};



#endif // EDITNOTESWORDPROPERTIESACTION_H
