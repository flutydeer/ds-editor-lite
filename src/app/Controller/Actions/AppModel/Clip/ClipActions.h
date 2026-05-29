//
// Created by fluty on 2024/2/8.
//

#ifndef CLIPACTIONS_H
#define CLIPACTIONS_H

#include "Model/AppModel/Clip.h"
#include "Modules/History/ActionSequence.h"


class AudioClip;
class SingingClip;
class Track;

class ClipActions : public ActionSequence {
    Q_OBJECT

public:
    void insertClips(const QList<Clip *> &clips, Track *track);
    void removeClips(const QList<Clip *> &clips, const QList<Track *> &tracks);
    void editSingingClipProperties(const QList<Clip::ClipCommonProperties> &oldArgs,
                                   const QList<Clip::ClipCommonProperties> &newArgs,
                                   const QList<SingingClip *> &clips, const QList<Track *> &tracks);
    void editAudioClipProperties(const QList<Clip::ClipCommonProperties> &oldArgs,
                                 const QList<Clip::ClipCommonProperties> &newArgs,
                                 const QList<AudioClip *> &clips, const QList<Track *> &tracks);
    void moveClipToTrack(const Clip::ClipCommonProperties &oldArgs,
                         const Clip::ClipCommonProperties &newArgs,
                         Clip *clip, Track *oldTrack, Track *newTrack);
};



#endif // CLIPACTIONS_H
