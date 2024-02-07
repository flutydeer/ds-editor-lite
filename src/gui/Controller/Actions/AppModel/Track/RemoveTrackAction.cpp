//
// Created by fluty on 2024/2/8.
//

#include "RemoveTrackAction.h"
RemoveTrackAction *RemoveTrackAction::build(DsTrack *track, int index, AppModel *model) {
    auto a = new RemoveTrackAction;
    a->m_track = track;
    a->m_index = index;
    a->m_model = model;
    return a;
}
void RemoveTrackAction::execute() {
    m_model->removeTrack(m_index);
}
void RemoveTrackAction::undo() {
    m_model->insertTrack(m_track, m_index);
}