//
// Created by fluty on 2024/2/8.
//

#ifndef EDITNOTESTARTACTION_H
#define EDITNOTESTARTACTION_H

#include "Modules/History/IAction.h"

#include <QList>

class SingingClip;
class Note;

class EditNoteStartAndLengthAction final : public IAction {
public:
    explicit EditNoteStartAndLengthAction(const QList<Note *> &notes, int deltaTick,
                                          SingingClip *clip)
        : m_notes(notes), m_deltaTick(deltaTick), m_clip(clip){};

    void execute() override;
    void undo() override;

private:
    QList<Note *> m_notes;
    int m_deltaTick = 0;
    SingingClip *m_clip = nullptr;
};



#endif // EDITNOTESTARTACTION_H
