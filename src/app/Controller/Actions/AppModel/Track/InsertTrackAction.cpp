//
// Created by fluty on 2024/2/8.
//

#include "InsertTrackAction.h"

#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/Track.h"

InsertTrackAction *InsertTrackAction::build(Track *track, const qsizetype index, AppModel *model) {
    const auto a = new InsertTrackAction;
    a->m_track = track;
    a->m_ownedTrack.reset(track);
    a->m_index = index;
    a->m_model = model;
    return a;
}

InsertTrackAction::~InsertTrackAction() = default;

void InsertTrackAction::execute() {
    if (m_ownedTrack)
        m_ownedTrack.release();
    m_model->insertTrack(m_track, m_index);
}

void InsertTrackAction::undo() {
    m_ownedTrack.reset(m_model->takeTrack(m_track));
}
