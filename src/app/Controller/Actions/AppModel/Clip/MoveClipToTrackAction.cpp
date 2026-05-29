//
// Created by fluty on 2024/2/8.
//

#include "MoveClipToTrackAction.h"

#include "Model/AppModel/Track.h"

MoveClipToTrackAction *MoveClipToTrackAction::build(const Clip::ClipCommonProperties &oldArgs,
                                                     const Clip::ClipCommonProperties &newArgs,
                                                     Clip *clip, Track *oldTrack,
                                                     Track *newTrack) {
    const auto a = new MoveClipToTrackAction;
    a->m_oldArgs = oldArgs;
    a->m_newArgs = newArgs;
    a->m_clip = clip;
    a->m_oldTrack = oldTrack;
    a->m_newTrack = newTrack;
    return a;
}

void MoveClipToTrackAction::execute() {
    m_oldTrack->removeClip(m_clip);
    m_clip->setName(m_newArgs.name);
    m_clip->setStart(m_newArgs.start);
    m_clip->setClipStart(m_newArgs.clipStart);
    m_clip->setLength(m_newArgs.length);
    m_clip->setClipLen(m_newArgs.clipLen);
    m_newTrack->insertClip(m_clip);
    m_clip->notifyPropertyChanged();
    m_oldTrack->notifyClipChanged(Track::Removed, m_clip);
    m_newTrack->notifyClipChanged(Track::Inserted, m_clip);
}

void MoveClipToTrackAction::undo() {
    m_newTrack->removeClip(m_clip);
    m_clip->setName(m_oldArgs.name);
    m_clip->setStart(m_oldArgs.start);
    m_clip->setClipStart(m_oldArgs.clipStart);
    m_clip->setLength(m_oldArgs.length);
    m_clip->setClipLen(m_oldArgs.clipLen);
    m_oldTrack->insertClip(m_clip);
    m_clip->notifyPropertyChanged();
    m_newTrack->notifyClipChanged(Track::Removed, m_clip);
    m_oldTrack->notifyClipChanged(Track::Inserted, m_clip);
}
