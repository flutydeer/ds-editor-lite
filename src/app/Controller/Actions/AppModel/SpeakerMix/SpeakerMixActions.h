#ifndef SPEAKERMIXACTIONS_H
#define SPEAKERMIXACTIONS_H

#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SpeakerMixData.h>
#include <lite/History/ActionSequence.h>

class SingingClip;
class Track;
class SingerInfo;
class SpeakerInfo;
using SpeakerMixModel::SpeakerMixData;

/// Note-level word data a track voice-context change invalidates in one clip.
struct ClipWordCascade {
    SingingClip *clip = nullptr;
    QList<Note *> notes;
    QList<Note::WordProperties> wordProperties;
    QList<QString> oldResolvedLanguages;
    QList<QString> newResolvedLanguages;
};

class SpeakerMixActions : public ActionSequence {
public:
    void replaceSpeakerMix(const SpeakerMixData &data, SingingClip *clip);
    void useTrackVoiceContext(SingingClip *clip);
    void selectClipSingleSpeaker(const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo,
                                 SingingClip *clip);
    void enableClipDynamicSpeakerMix(const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo,
                                     const SpeakerMixData &data, SingingClip *clip,
                                     const ClipWordCascade &cascade);
    void applyClipSpeakerMixPreset(const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo,
                                   const SpeakerMixData &data, SingingClip *clip,
                                   const ClipWordCascade &cascade);
    void selectTrackSingleSpeaker(const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo,
                                  Track *track);
    void applyTrackSpeakerMixPreset(const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo,
                                    const SpeakerMixData &data, Track *track);
    void replaceTrackSpeakerMix(const SpeakerMixData &data, Track *track);

    /// Track voice-context variants that also cascade into the clips following
    /// the track. Clips with their own voice source are not in `cascades`.
    void selectTrackSingleSpeaker(const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo,
                                  Track *track, const QList<ClipWordCascade> &cascades);
    void applyTrackSpeakerMixPreset(const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo,
                                    const SpeakerMixData &data, Track *track,
                                    const QList<ClipWordCascade> &cascades);
    void replaceTrackSpeakerMix(const SpeakerMixData &data, Track *track,
                                const QList<ClipWordCascade> &cascades);

    /// Switches the clip voice context and rewrites note word properties in one
    /// undo step.
    ///
    /// `cascade` is already planned against the target context, and its resolved
    /// languages tell the note action whether the change invalidated derived data.
    void useTrackVoiceContext(SingingClip *clip, const ClipWordCascade &cascade);
    void selectClipSingleSpeaker(const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo,
                                 SingingClip *clip, const ClipWordCascade &cascade);

private:
    void addWordCascades(const QList<ClipWordCascade> &cascades);
    void switchClipVoiceContext(bool useTrackVoiceContext, const SingerInfo &singerInfo,
                                const SpeakerInfo &speakerInfo, const SpeakerMixData &speakerMix,
                                SingingClip *clip, const ClipWordCascade &cascade);
};

#endif // SPEAKERMIXACTIONS_H
