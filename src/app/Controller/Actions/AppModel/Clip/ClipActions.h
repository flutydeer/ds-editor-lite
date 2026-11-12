#ifndef CLIPACTIONS_H
#define CLIPACTIONS_H

#include <lite/ProjectModel/AppModel/Clip.h>
#include <lite/History/ActionSequence.h>

#include <QJsonObject>

struct AudioPathInfo;
class AudioClip;
class SingingClip;
class Track;

// One clip property edit as a single history step. `newTrack` differs from `oldTrack` only when the
// edit also moves the clip to another track, which needs the dedicated cross-track action instead
// of a plain property write.
struct ClipPropertyEdit {
    Clip::ClipCommonProperties oldArgs;
    Clip::ClipCommonProperties newArgs;
    Clip *clip = nullptr;
    Track *oldTrack = nullptr;
    Track *newTrack = nullptr;
};

class ClipActions : public ActionSequence {
    Q_OBJECT

public:
    void insertClips(const QList<Clip *> &clips, Track *track);
    void insertClips(const QList<Clip *> &clips, const QList<Track *> &tracks);
    void removeClips(const QList<Clip *> &clips, const QList<Track *> &tracks);
    // Batch edit of one history step. The history name covers the whole batch and the focus
    // transition wraps every edited clip, so undo reveals all of them at once, whatever the mix of
    // singing and audio clips or of same-track and cross-track edits.
    void editClips(const QList<ClipPropertyEdit> &edits);
    void editSingingClipProperties(const QList<Clip::ClipCommonProperties> &oldArgs,
                                   const QList<Clip::ClipCommonProperties> &newArgs,
                                   const QList<SingingClip *> &clips, const QList<Track *> &tracks);
    void editAudioClipProperties(const QList<Clip::ClipCommonProperties> &oldArgs,
                                 const QList<Clip::ClipCommonProperties> &newArgs,
                                 const QList<AudioClip *> &clips, const QList<Track *> &tracks);
    void splitClip(const Clip::ClipCommonProperties &oldArgs,
                   const Clip::ClipCommonProperties &newArgs, Clip *clip, Clip *newClip,
                   Track *track);
    void relocateAudioClip(AudioClip *clip, const QString &newPath,
                           const AudioPathInfo &newPathInfo, const QJsonObject &newFormatData);
    void moveClipToTrack(const Clip::ClipCommonProperties &oldArgs,
                         const Clip::ClipCommonProperties &newArgs, Clip *clip, Track *oldTrack,
                         Track *newTrack);
};



#endif // CLIPACTIONS_H
