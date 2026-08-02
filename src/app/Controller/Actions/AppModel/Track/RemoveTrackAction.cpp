#include "RemoveTrackAction.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Track.h>

RemoveTrackAction *RemoveTrackAction::build(Track *track, AppModel *model) {
    const auto a = new RemoveTrackAction;
    a->m_track = track;
    a->m_model = model;
    a->m_index = model->tracks().indexOf(track);
    return a;
}

RemoveTrackAction::~RemoveTrackAction() = default;

void RemoveTrackAction::execute() {
    const auto index = m_model->tracks().indexOf(m_track);
    if (index < 0)
        return;
    const auto track = m_model->takeTrack(m_track);
    if (!track)
        return;
    m_index = index;
    m_ownedTrack.reset(track);
    m_model->notifyTrackChanged(AppModel::Remove, index, track);
}

void RemoveTrackAction::undo() {
    if (!m_ownedTrack)
        return;
    const auto track = m_ownedTrack.get();
    if (!m_model->insertTrack(track, m_index))
        return;
    m_ownedTrack.release();
    m_model->notifyTrackChanged(AppModel::Insert, m_index, track);
}
