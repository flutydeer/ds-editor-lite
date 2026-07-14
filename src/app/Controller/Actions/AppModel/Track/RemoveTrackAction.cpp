//
// Created by fluty on 2024/2/8.
//

#include "RemoveTrackAction.h"

#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/Track.h"

RemoveTrackAction *RemoveTrackAction::build(Track *track, AppModel *model) {
    const auto a = new RemoveTrackAction;
    a->m_track = track;
    a->m_model = model;
    a->m_index = model->tracks().indexOf(track);
    return a;
}

RemoveTrackAction::~RemoveTrackAction() = default;

void RemoveTrackAction::execute() {
    m_ownedTrack.reset(m_model->takeTrack(m_track));
}

void RemoveTrackAction::undo() {
    if (!m_ownedTrack)
        return;
    m_model->insertTrack(m_ownedTrack.release(), m_index);
}
