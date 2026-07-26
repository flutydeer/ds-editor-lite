//
// Created by fluty on 2024/2/8.
//

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
    if (m_ownedTrack)
        m_ownedTrack.release();
    // Imported tracks may carry audio clips whose ticks are authoritative;
    // establish the realtime anchor under the timeline in effect now
    for (const auto clip : m_track->clips()) {
        if (clip->clipType() != IClip::Audio)
            continue;
        const auto audioClip = static_cast<AudioClip *>(clip);
        if (!audioClip->hasRealTimeAnchor())
            audioClip->syncTruthFromTicks(m_model->timeline());
    }
    m_model->insertTrack(m_track, m_index);
}

void InsertTrackAction::undo() {
    m_ownedTrack.reset(m_model->takeTrack(m_track));
}
