//
// Created by fluty on 2024/2/8.
//

#include "TrackActions.h"


#include "AppendTrackAction.h"
#include "EditTrackPropertiesAction.h"
#include "InsertTrackAction.h"
#include "RemoveTrackAction.h"
void TrackActions::appendTracks(const QList<DsTrack *> &tracks, AppModel *model) {
    for (auto track : tracks)
        addAction(AppendTrackAction::build(track, model));
}
void TrackActions::insertTrack(DsTrack *track, int index, AppModel *model) {
    addAction(InsertTrackAction::build(track, index, model));
}
void TrackActions::removeTracks(const QList<DsTrack *> &tracks, AppModel *model) {
    for (auto track : tracks)
        addAction(RemoveTrackAction::build(track, model));
}
void TrackActions::editTrackProperties(const DsTrack::TrackProperties &oldArgs,
                                       const DsTrack::TrackProperties &newArgs, DsTrack *track) {
    addAction(EditTrackPropertiesAction::build(oldArgs, newArgs, track));
}