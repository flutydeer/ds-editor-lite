#include "ResetPhonemeOffsetsAction.h"

#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

void ResetPhonemeOffsetsAction::execute() {
    m_resetRecords.clear();
    for (const auto note : m_notes) {
        if (!note)
            continue;
        const auto &seq = note->phonemeOffsetSeq();
        if (!seq.isEdited())
            continue;
        SingingClipPhonemeNormalizer::ResetRecord record;
        record.note = note;
        record.editedOffsets = seq.edited;
        m_resetRecords.append(record);
        note->setPhonemeOffsetSeq(Note::Edited, {});
    }
    if (!m_resetRecords.isEmpty())
        m_clip->notifyNoteChanged(
            SingingClip::EditedPhonemeOffsetChange,
            SingingClipPhonemeNormalizer::notesFromResetRecords(m_resetRecords));
}

void ResetPhonemeOffsetsAction::undo() {
    SingingClipPhonemeNormalizer::restoreEditedOffsets(m_resetRecords);
    if (!m_resetRecords.isEmpty())
        m_clip->notifyNoteChanged(
            SingingClip::EditedPhonemeOffsetChange,
            SingingClipPhonemeNormalizer::notesFromResetRecords(m_resetRecords));
}
