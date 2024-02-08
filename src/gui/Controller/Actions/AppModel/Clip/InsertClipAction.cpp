//
// Created by fluty on 2024/2/8.
//

#include "InsertClipAction.h"
InsertClipAction *InsertClipAction::build(DsClip *clip, DsTrack *track) {
    auto a = new InsertClipAction;
    a->m_clip = clip;
    a->m_track = track;
    return a;
}
void InsertClipAction::execute() {
    m_track->insertClip(m_clip);
}
void InsertClipAction::undo() {
    m_track->removeClip(m_clip);
}