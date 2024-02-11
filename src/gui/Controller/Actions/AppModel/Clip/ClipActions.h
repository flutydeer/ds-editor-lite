//
// Created by fluty on 2024/2/8.
//

#ifndef CLIPACTIONS_H
#define CLIPACTIONS_H

#include "Controller/History/ActionSequence.h"
#include "Model/Clip.h"
#include "Model/Track.h"

class ClipActions : public ActionSequence {
public:
    void insertClips(const QList<Clip *> &clips, Track *track);
    void removeClips(const QList<Clip *> &clips, Track *track);
    void editSingingClipProperties(const QList<Clip::ClipCommonProperties> &oldArgs,
                                   const QList<Clip::ClipCommonProperties> &newArgs,
                                   const QList<DsSingingClip *> &clips, Track *track);
    void editAudioClipProperties(const QList<Clip::ClipCommonProperties> &oldArgs,
                                   const QList<Clip::ClipCommonProperties> &newArgs,
                                   const QList<DsAudioClip *> &clips, Track *track);
};



#endif // CLIPACTIONS_H
