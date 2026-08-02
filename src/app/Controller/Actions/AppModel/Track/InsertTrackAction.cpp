#include "InsertTrackAction.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

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
    if (!m_track || m_index < 0 || m_index > m_model->tracks().size() ||
        m_model->tracks().contains(m_track))
        return;

    // Imported tracks may carry audio clips whose ticks are authoritative;
    // establish the realtime anchor under the timeline in effect now
    for (const auto clip : m_track->clips()) {
        if (clip->clipType() != IClip::Audio)
            continue;
        const auto audioClip = static_cast<AudioClip *>(clip);
        if (!audioClip->hasRealTimeAnchor())
            audioClip->syncTruthFromTicks(m_model->timeline());
    }
    if (!m_model->insertTrack(m_track, m_index))
        return;
    m_ownedTrack.release();
    m_model->notifyTrackChanged(AppModel::Insert, m_index, m_track);
}

void InsertTrackAction::undo() {
    const auto index = m_model->tracks().indexOf(m_track);
    if (index < 0)
        return;
    const auto track = m_model->takeTrack(m_track);
    if (!track)
        return;
    m_ownedTrack.reset(track);
    m_model->notifyTrackChanged(AppModel::Remove, index, track);
}
