#ifndef RESETPHONEMEOFFSETSACTION_H
#define RESETPHONEMEOFFSETSACTION_H

#include <lite/History/IAction.h>
#include "Model/AppModel/SingingClipPhonemeNormalizer.h"

#include <QList>

class SingingClip;
class Note;

// Clears the manually edited phoneme offsets (edited) of the given word roots,
// restoring the model baseline (original). One action records the full cascade
// closure so undo restores every touched note, including cascade neighbors.
class ResetPhonemeOffsetsAction final : public IAction {
public:
    explicit ResetPhonemeOffsetsAction(const QList<Note *> &notes, SingingClip *clip)
        : m_notes(notes), m_clip(clip) {};
    void execute() override;
    void undo() override;

private:
    QList<Note *> m_notes;
    QList<SingingClipPhonemeNormalizer::ResetRecord> m_resetRecords;
    SingingClip *m_clip = nullptr;
};

#endif // RESETPHONEMEOFFSETSACTION_H
