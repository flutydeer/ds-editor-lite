//
// Created by fluty on 2024/2/8.
//

#ifndef CLIPACTIONS_H
#define CLIPACTIONS_H

#include "Controller/History/ActionSequence.h"
#include "Model/DsClip.h"
#include "Model/DsTrack.h"

class ClipActions : public ActionSequence {
public:
    void insertClips(const QList<DsClip *> &clips, DsTrack *track);
    void removeClips(const QList<DsClip *> &clips, DsTrack *track);
    void editSingingClipProperties(const QList<DsClip::ClipCommonProperties> &oldArgs,
                                   const QList<DsClip::ClipCommonProperties> &newArgs,
                                   const QList<DsSingingClip *> &clips, DsTrack *track);
    void editAudioClipProperties(const QList<DsClip::ClipCommonProperties> &oldArgs,
                                   const QList<DsClip::ClipCommonProperties> &newArgs,
                                   const QList<DsAudioClip *> &clips, DsTrack *track);
};



#endif // CLIPACTIONS_H
