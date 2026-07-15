//
// Created by fluty on 2024/2/8.
//

#include "AppendTrackAction.h"

#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/Track.h"

AppendTrackAction *AppendTrackAction::build(Track *track, AppModel *model) {
    auto a = new AppendTrackAction;
    a->m_track = track;
    a->m_ownedTrack.reset(track);
    a->m_model = model;
    return a;
}

AppendTrackAction::~AppendTrackAction() = default;

void AppendTrackAction::execute() {
    if (m_ownedTrack)
        m_ownedTrack.release();
    m_model->appendTrack(m_track);
}

void AppendTrackAction::undo() {
    m_ownedTrack.reset(m_model->takeTrack(m_track));
}
