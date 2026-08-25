#ifndef TRACKACTIONS_H
#define TRACKACTIONS_H

#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/History/ActionSequence.h>

class AppModel;

class TrackActions : public ActionSequence {
public:
    void appendTracks(const QList<Track *> &tracks, AppModel *model);
    void insertTrack(Track *track, qsizetype index, AppModel *model,
                     bool resolveColorIndex = true);
    void removeTracks(const QList<Track *> &tracks, AppModel *model);
    void editTrackProperties(const Track::TrackProperties &oldArgs,
                             const Track::TrackProperties &newArgs, Track *track);
    void moveTrack(qsizetype fromIndex, qsizetype toIndex, AppModel *model);
};



#endif // TRACKACTIONS_H
