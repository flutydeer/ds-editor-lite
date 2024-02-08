//
// Created by fluty on 2024/2/8.
//

#include "EditSingingClipPropertiesAction.h"
EditSingingClipPropertiesAction *
    EditSingingClipPropertiesAction::build(const DsClip::ClipCommonProperties &oldArgs,
                                     const DsClip::ClipCommonProperties &newArgs,
                                     DsSingingClip *clip, DsTrack *track) {
    auto a = new EditSingingClipPropertiesAction;
    a->m_oldArgs = oldArgs;
    a->m_newArgs = newArgs;
    a->m_clip = clip;
    a->m_track = track;
    return a;
}
void EditSingingClipPropertiesAction::execute() {
    m_track->removeClipQuietly(m_clip);

    auto deltaTick = m_newArgs.start - m_oldArgs.start;
    if (deltaTick != 0)
        for (auto note : m_clip->notes())
            note->setStart(note->start() + deltaTick);

    m_clip->setName(m_newArgs.name);
    m_clip->setStart(m_newArgs.start);
    m_clip->setClipStart(m_newArgs.clipStart);
    m_clip->setLength(m_newArgs.length);
    m_clip->setClipLen(m_newArgs.clipLen);

    m_track->insertClipQuietly(m_clip);
    m_track->notityClipPropertyChanged(m_clip);
}
void EditSingingClipPropertiesAction::undo() {
    m_track->removeClipQuietly(m_clip);

    auto deltaTick = m_newArgs.start - m_oldArgs.start;
    if (deltaTick != 0)
        for (auto note : m_clip->notes())
            note->setStart(note->start() - deltaTick);

    m_clip->setName(m_oldArgs.name);
    m_clip->setStart(m_oldArgs.start);
    m_clip->setClipStart(m_oldArgs.clipStart);
    m_clip->setLength(m_oldArgs.length);
    m_clip->setClipLen(m_oldArgs.clipLen);

    m_track->insertClipQuietly(m_clip);
    m_track->notityClipPropertyChanged(m_clip);
}