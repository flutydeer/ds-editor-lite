//
// Created by fluty on 2024/2/8.
//

#ifndef EDITNOTESWORDPROPERTIESACTION_H
#define EDITNOTESWORDPROPERTIESACTION_H

#include "Controller/History/IAction.h"
#include "Model/DsClip.h"

class EditNotesWordPropertiesAction final : public IAction {
public:
    static EditNotesWordPropertiesAction *build(DsNote *note, const DsNote::NoteWordProperties &args);
    void execute() override;
    void undo() override;

private:
    DsNote *m_note = nullptr;
    DsNote::NoteWordProperties m_oldArgs;
    DsNote::NoteWordProperties m_newArgs;
};



#endif //EDITNOTESWORDPROPERTIESACTION_H
