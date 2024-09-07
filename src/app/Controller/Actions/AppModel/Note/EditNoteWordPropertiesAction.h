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
    explicit EditNoteWordPropertiesAction(const QList<Note *> &notes,
                                          const QList<Note::WordProperties> &args,
                                          SingingClip *clip);
    void execute() override;
    void undo() override;

private:
    QList<Note *> m_notes;
    QList<Note::WordProperties> m_oldArgs;
    QList<Note::WordProperties> m_newArgs;
    SingingClip *m_clip = nullptr;
};



#endif // EDITNOTESWORDPROPERTIESACTION_H
