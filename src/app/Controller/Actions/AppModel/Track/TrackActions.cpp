//
// Created by fluty on 2024/2/8.
//

#include "TrackActions.h"

#include "AppendTrackAction.h"
#include "EditTrackPropertiesAction.h"
#include "InsertTrackAction.h"
#include "RemoveTrackAction.h"

void TrackActions::appendTracks(const QList<Track *> &tracks, AppModel *model) {
    for (auto track : tracks)
        addAction(AppendTrackAction::build(track, model));
}
void TrackActions::insertTrack(Track *track, qsizetype index, AppModel *model) {
    addAction(InsertTrackAction::build(track, index, model));
}
void TrackActions::removeTracks(const QList<Track *> &tracks, AppModel *model) {
    for (auto track : tracks)
        addAction(RemoveTrackAction::build(track, model));
}
void TrackActions::editTrackProperties(const Track::TrackProperties &oldArgs,
                                       const Track::TrackProperties &newArgs, Track *track) {
    addAction(EditTrackPropertiesAction::build(oldArgs, newArgs, track));
}