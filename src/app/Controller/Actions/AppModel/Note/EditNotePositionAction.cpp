#include "EditNotePositionAction.h"

#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

void EditNotePositionAction::execute() {
    QHash<Note *, int> startDeltas;
    if (m_deltaTick != 0) {
        for (const auto note : m_notes)
            startDeltas.insert(note, m_deltaTick);
        m_resetRecords =
            SingingClipPhonemeNormalizer::normalizeEditedOffsets(*m_clip, startDeltas);
    } else {
        m_resetRecords.clear();
    }

    for (const auto &note : m_notes) {
        m_clip->removeNote(note);
        note->setLocalStart(note->localStart() + m_deltaTick);
        note->setKeyIndex(note->keyIndex() + m_deltaKey);
        m_clip->insertNote(note);
    }
    m_resetRecords.append(startDeltas.isEmpty()
                              ? SingingClipPhonemeNormalizer::normalizeEditedOffsets(*m_clip)
                              : SingingClipPhonemeNormalizer::normalizeEditedOffsets(*m_clip,
                                                                                     startDeltas));
    m_clip->notifyNoteChanged(SingingClip::TimeKeyPropertyChange, m_notes);
    if (!m_resetRecords.isEmpty())
        m_clip->notifyNoteChanged(
            SingingClip::EditedPhonemeOffsetChange,
            SingingClipPhonemeNormalizer::notesFromResetRecords(m_resetRecords));
}

void EditNotePositionAction::undo() {
    for (const auto &note : m_notes) {
        m_clip->removeNote(note);
        note->setLocalStart(note->localStart() - m_deltaTick);
        note->setKeyIndex(note->keyIndex() - m_deltaKey);
        m_clip->insertNote(note);
    }
    SingingClipPhonemeNormalizer::restoreEditedOffsets(m_resetRecords);
    m_clip->notifyNoteChanged(SingingClip::TimeKeyPropertyChange, m_notes);
    if (!m_resetRecords.isEmpty())
        m_clip->notifyNoteChanged(
            SingingClip::EditedPhonemeOffsetChange,
            SingingClipPhonemeNormalizer::notesFromResetRecords(m_resetRecords));
}
