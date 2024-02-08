//
// Created by fluty on 2024/2/8.
//

#include "InsertTrackAction.h"
InsertTrackAction *InsertTrackAction::build(DsTrack *track, int index, AppModel *model) {
    auto a = new InsertTrackAction;
    a->m_track = track;
    a->m_index = index;
    a->m_model = model;
    return a;
}
void InsertTrackAction::execute() {
    m_model->insertTrack(m_track, m_index);
}
void InsertTrackAction::undo() {
    m_model->removeTrackAt(m_index);
}