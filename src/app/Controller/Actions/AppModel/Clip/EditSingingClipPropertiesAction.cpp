#include "EditSingingClipPropertiesAction.h"

#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

EditSingingClipPropertiesAction *
    EditSingingClipPropertiesAction::build(const Clip::ClipCommonProperties &oldArgs,
                                           const Clip::ClipCommonProperties &newArgs,
                                           SingingClip *clip, Track *track) {
    const auto a = new EditSingingClipPropertiesAction;
    a->m_oldArgs = oldArgs;
    a->m_newArgs = newArgs;
    a->m_clip = clip;
    a->m_track = track;
    return a;
}

void EditSingingClipPropertiesAction::execute() {
    const auto previousGroupStates =
        SingingClipPhonemeNormalizer::captureGroupStates(*m_clip);
    m_track->removeClip(m_clip);
    m_clip->setName(m_newArgs.name);
    m_clip->setStart(m_newArgs.start);
    m_clip->setClipStart(m_newArgs.clipStart);
    m_clip->setLength(m_newArgs.length);
    m_clip->setClipLen(m_newArgs.clipLen);
    m_track->insertClip(m_clip);
    m_resetRecords = previousGroupStates.isEmpty()
                         ? QList<SingingClipPhonemeNormalizer::ResetRecord>{}
                         : SingingClipPhonemeNormalizer::normalizeEditedOffsets(
                               *m_clip, previousGroupStates);
    m_clip->notifyPropertyChanged();
    if (!m_resetRecords.isEmpty())
        m_clip->notifyNoteChanged(
            SingingClip::EditedPhonemeOffsetChange,
            SingingClipPhonemeNormalizer::notesFromResetRecords(m_resetRecords));
}

void EditSingingClipPropertiesAction::undo() {
    m_track->removeClip(m_clip);
    m_clip->setName(m_oldArgs.name);
    m_clip->setStart(m_oldArgs.start);
    m_clip->setClipStart(m_oldArgs.clipStart);
    m_clip->setLength(m_oldArgs.length);
    m_clip->setClipLen(m_oldArgs.clipLen);
    m_track->insertClip(m_clip);
    SingingClipPhonemeNormalizer::restoreEditedOffsets(m_resetRecords);
    m_clip->notifyPropertyChanged();
    if (!m_resetRecords.isEmpty())
        m_clip->notifyNoteChanged(
            SingingClip::EditedPhonemeOffsetChange,
            SingingClipPhonemeNormalizer::notesFromResetRecords(m_resetRecords));
}
