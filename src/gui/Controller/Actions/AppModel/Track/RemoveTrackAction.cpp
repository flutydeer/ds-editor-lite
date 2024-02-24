//
// Created by fluty on 2024/2/8.
//

#include "RemoveTrackAction.h"

#include "Model/AppModel.h"

RemoveTrackAction *RemoveTrackAction::build(Track *track, AppModel *model) {
    auto a = new RemoveTrackAction;
    a->m_track = track;
    a->m_model = model;
    a->m_originalTracks = model->tracks();
    return a;
}
void RemoveTrackAction::execute() {
    m_model->removeTrack(m_track);
}
void RemoveTrackAction::undo() {
    m_model->clearTracks();
    for (const auto track : m_originalTracks)
        m_model->appendTrack(track);
}