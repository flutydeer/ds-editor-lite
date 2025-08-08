//
// Created by fluty on 2024/2/8.
//

#ifndef EDITNOTESLENGTHACTION_H
#define EDITNOTESLENGTHACTION_H

#include "Modules/History/IAction.h"

#include <QList>

class SingingClip;
class Note;

class EditNotesLengthAction final : public IAction {
public:
    explicit EditNotesLengthAction(const QList<Note *> &notes, const int deltaTick,
                                   SingingClip *clip)
        : m_notes(notes), m_deltaTick(deltaTick), m_clip(clip) {};
    void execute() override;
    void undo() override;

private:
    QList<Note *> m_notes;
    int m_deltaTick = 0;
    SingingClip *m_clip = nullptr;
};



#endif // EDITNOTESLENGTHACTION_H
