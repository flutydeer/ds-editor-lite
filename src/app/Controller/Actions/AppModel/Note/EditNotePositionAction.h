//
// Created by fluty on 2024/2/8.
//

#ifndef EDITNOTEPOSITIONACTION_H
#define EDITNOTEPOSITIONACTION_H

#include "Modules/History/IAction.h"

#include <QList>

class SingingClip;
class Note;

class EditNotePositionAction final : public IAction {
public:
    explicit EditNotePositionAction(const QList<Note *> &notes, int deltaTick, int deltaKey,
                                    SingingClip *clip)
        : m_notes(notes), m_deltaTick(deltaTick), m_deltaKey(deltaKey), m_clip(clip){};
    void execute() override;
    void undo() override;

private:
    QList<Note *> m_notes;
    int m_deltaTick = 0;
    int m_deltaKey = 0;
    SingingClip *m_clip = nullptr;
};



#endif // EDITNOTEPOSITIONACTION_H
