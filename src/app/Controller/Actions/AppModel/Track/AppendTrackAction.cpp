//
// Created by fluty on 2024/2/8.
//

#include "AppendTrackAction.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Track.h>

AppendTrackAction *AppendTrackAction::build(Track *track, AppModel *model) {
    auto a = new AppendTrackAction;
    a->m_track = track;
    a->m_ownedTrack.reset(track);
    a->m_model = model;
    return a;
}

AppendTrackAction::~AppendTrackAction() = default;

void AppendTrackAction::execute() {
    const auto index = m_model->tracks().size();
    if (!m_model->appendTrack(m_track))
        return;
    m_ownedTrack.release();
    m_model->notifyTrackChanged(AppModel::Insert, index, m_track);
}

void AppendTrackAction::undo() {
    const auto index = m_model->tracks().indexOf(m_track);
    if (index < 0)
        return;
    const auto track = m_model->takeTrack(m_track);
    if (!track)
        return;
    m_ownedTrack.reset(track);
    m_model->notifyTrackChanged(AppModel::Remove, index, track);
}
