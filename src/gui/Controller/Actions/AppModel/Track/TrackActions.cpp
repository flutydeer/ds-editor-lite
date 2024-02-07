//
// Created by fluty on 2024/2/8.
//

#include "TrackActions.h"


#include "InsertTrackAction.h"
#include "RemoveTrackAction.h"
void TrackActions::insertTrack(DsTrack *track, int index, AppModel *model) {
    auto a = InsertTrackAction::build(track, index, model);
    m_actionSequence.append(a);
}
// void TrackActions::removeTracks(const QList<DsTrack *> &tracks, AppModel *model) {
//     for (auto track : tracks) {
//         auto a = RemoveTrackAction::build(track, )
//     }
// }
void TrackActions::removeTrack(DsTrack *track, int index, AppModel *model) {
    auto a = RemoveTrackAction::build(track, index, model);
    m_actionSequence.append(a);
}