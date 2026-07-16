//
// Created by fluty on 2024/2/8.
//

#ifndef CLIPACTIONS_H
#define CLIPACTIONS_H

#include "Model/AppModel/Clip.h"
#include "Modules/History/ActionSequence.h"

#include <QJsonObject>

struct AudioPathInfo;
class AudioClip;
class SingingClip;
class Track;

class ClipActions : public ActionSequence {
    Q_OBJECT

public:
    void insertClips(const QList<Clip *> &clips, Track *track);
    void insertClips(const QList<Clip *> &clips, const QList<Track *> &tracks);
    void removeClips(const QList<Clip *> &clips, const QList<Track *> &tracks);
    void editSingingClipProperties(const QList<Clip::ClipCommonProperties> &oldArgs,
                                   const QList<Clip::ClipCommonProperties> &newArgs,
                                   const QList<SingingClip *> &clips, const QList<Track *> &tracks);
    void editAudioClipProperties(const QList<Clip::ClipCommonProperties> &oldArgs,
                                 const QList<Clip::ClipCommonProperties> &newArgs,
                                 const QList<AudioClip *> &clips, const QList<Track *> &tracks);
    void relocateAudioClip(AudioClip *clip, const QString &newPath,
                           const AudioPathInfo &newPathInfo, const QJsonObject &newFormatData);
    void moveClipToTrack(const Clip::ClipCommonProperties &oldArgs,
                         const Clip::ClipCommonProperties &newArgs,
                         Clip *clip, Track *oldTrack, Track *newTrack);
};



#endif // CLIPACTIONS_H
