//
// Created by fluty on 2024/2/8.
//

#ifndef INSERTNOTEACTION_H
#define INSERTNOTEACTION_H

#include "Modules/History/IAction.h"
#include "Model/AppModel/SingingClipPhonemeNormalizer.h"

#include <QList>

class SingingClip;
class Note;

class InsertNoteAction final : public IAction {
public:
    explicit InsertNoteAction(const QList<Note *> &notes, SingingClip *clip)
        : m_notes(notes), m_clip(clip) {};
    void execute() override;
    void undo() override;

private:
    QList<Note *> m_notes;
    QList<SingingClipPhonemeNormalizer::ResetRecord> m_resetRecords;
    SingingClip *m_clip = nullptr;
};



#endif // INSERTNOTEACTION_H
