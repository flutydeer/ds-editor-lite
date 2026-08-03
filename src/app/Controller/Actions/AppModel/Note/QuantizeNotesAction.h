#ifndef QUANTIZENOTESACTION_H
#define QUANTIZENOTESACTION_H

#include <lite/History/IAction.h>
#include "Model/AppModel/SingingClipPhonemeNormalizer.h"

#include <QList>
#include <QPair>

#include <utility>

class SingingClip;
class Note;

// Sets absolute start/length values (unlike the delta-based edit actions),
// so each note can move by a different amount — required by quantize.
class QuantizeNotesAction final : public IAction {
public:
    struct Change {
        Note *note = nullptr;
        int oldStart = 0;
        int oldLength = 0;
        int newStart = 0;
        int newLength = 0;
    };

    explicit QuantizeNotesAction(QList<Change> changes, SingingClip *clip)
        : m_changes(std::move(changes)), m_clip(clip) {}

    void execute() override;
    void undo() override;

private:
    QList<Note *> notes() const;

    QList<Change> m_changes;
    QList<SingingClipPhonemeNormalizer::ResetRecord> m_resetRecords;
    SingingClip *m_clip = nullptr;
};

#endif // QUANTIZENOTESACTION_H
