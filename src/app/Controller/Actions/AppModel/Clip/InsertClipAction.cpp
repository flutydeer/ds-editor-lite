//
// Created by fluty on 2024/2/8.
//

#include "InsertClipAction.h"

#include "Model/AppModel/Track.h"

InsertClipAction *InsertClipAction::build(Clip *clip, Track *track) {
    const auto a = new InsertClipAction;
    a->m_clip = clip;
    a->m_track = track;
    return a;
}

void InsertClipAction::execute() {
    m_track->insertClip(m_clip);
    m_track->notifyClipChanged(Track::Inserted, m_clip);
}

void InsertClipAction::undo() {
    m_track->removeClip(m_clip);
    m_track->notifyClipChanged(Track::Removed, m_clip);
}