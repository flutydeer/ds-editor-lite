//
// Created by fluty on 2024/2/8.
//

#ifndef REMOVENOTEACTION_H
#define REMOVENOTEACTION_H

#include "Modules/History/IAction.h"

#include <QList>

class SingingClip;
class Note;

class RemoveNoteAction final : public IAction {
public:
    explicit RemoveNoteAction(const QList<Note *> &notes, SingingClip *clip)
        : m_notes(notes), m_clip(clip){};
    void execute() override;
    void undo() override;

private:
    QList<Note *> m_notes;
    SingingClip *m_clip = nullptr;
};



#endif // REMOVENOTEACTION_H
