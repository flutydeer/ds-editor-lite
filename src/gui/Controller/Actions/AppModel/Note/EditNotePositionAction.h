//
// Created by fluty on 2024/2/8.
//

#ifndef EDITNOTEPOSITIONACTION_H
#define EDITNOTEPOSITIONACTION_H

#include "Controller/History/IAction.h"
#include "Model/DsClip.h"

class EditNotePositionAction final : public IAction {
public:
    static EditNotePositionAction *build(DsNote *note, int deltaTick, int deltaKey,
                                         DsSingingClip *clip);
    void execute() override;
    void undo() override;

private:
    DsNote *m_note = nullptr;
    int m_deltaTick = 0;
    int m_deltaKey = 0;
    DsSingingClip *m_clip = nullptr;
};



#endif // EDITNOTEPOSITIONACTION_H
