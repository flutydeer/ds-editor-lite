//
// Created by fluty on 2024/2/8.
//

#ifndef TRACKACTIONS_H
#define TRACKACTIONS_H

#include "Controller/History/ActionSequence.h"
#include "Model/AppModel.h"

class TrackActions : public ActionSequence{
public:
    void insertTrack(DsTrack *track, int index, AppModel *model);
    // void removeTracks(const QList<DsTrack *> &tracks, AppModel *model);
    void removeTrack(DsTrack *track, int index, AppModel *model);
};



#endif //TRACKACTIONS_H
