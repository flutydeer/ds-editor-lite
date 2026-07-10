//
// Created for split note functionality
//

#ifndef SPLITNOTEACTION_H
#define SPLITNOTEACTION_H

#include "Modules/History/IAction.h"
#include "Model/AppModel/SingingClipPhonemeNormalizer.h"

#include <QList>

class SingingClip;
class Note;

class SplitNoteAction final : public IAction {
public:
    explicit SplitNoteAction(Note *originalNote, Note *newNote, int newLength, SingingClip *clip);
    void execute() override;
    void undo() override;

private:
    Note *m_originalNote = nullptr;
    Note *m_newNote = nullptr;
    QList<SingingClipPhonemeNormalizer::ResetRecord> m_resetRecords;
    int m_originalLength = 0;
    int m_newLength = 0;
    SingingClip *m_clip = nullptr;
};

#endif // SPLITNOTEACTION_H
