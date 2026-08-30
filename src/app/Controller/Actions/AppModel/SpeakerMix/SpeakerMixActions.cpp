#include "SpeakerMixActions.h"

#include "SetClipVoiceContextAction.h"
#include "SetTrackVoiceContextAction.h"

#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QCoreApplication>

using namespace SpeakerMixModel;

void SpeakerMixActions::replaceSpeakerMix(const SpeakerMixData &data, SingingClip *clip) {
    setTranslatableName("SpeakerMixActions",
                        QT_TRANSLATE_NOOP("SpeakerMixActions", "Edit speaker mix"));
    const auto oldContext = clip ? clip->effectiveVoiceContext() : EffectiveVoiceContext();
    const auto newData = preservePresetSourceAsDirty(oldContext.speakerMix, data);
    addAction(
        new SetClipVoiceContextAction(false, oldContext.singer, oldContext.speaker, newData, clip));
}

void SpeakerMixActions::useTrackVoiceContext(SingingClip *clip) {
    setTranslatableName("SpeakerMixActions",
                        QT_TRANSLATE_NOOP("SpeakerMixActions", "Use track singer"));
    addAction(new SetClipVoiceContextAction(true, clip->ownSingerInfo(), clip->ownSpeakerInfo(),
                                            clip->ownSpeakerMixData(), clip));
}

void SpeakerMixActions::selectClipSingleSpeaker(const SingerInfo &singerInfo,
                                                const SpeakerInfo &speakerInfo, SingingClip *clip) {
    setTranslatableName("SpeakerMixActions",
                        QT_TRANSLATE_NOOP("SpeakerMixActions", "Select speaker"));
    addAction(new SetClipVoiceContextAction(false, singerInfo, speakerInfo, {}, clip));
}

void SpeakerMixActions::enableClipDynamicSpeakerMix(const SingerInfo &singerInfo,
                                                    const SpeakerInfo &speakerInfo,
                                                    const SpeakerMixData &data, SingingClip *clip) {
    setTranslatableName("SpeakerMixActions",
                        QT_TRANSLATE_NOOP("SpeakerMixActions", "Enable dynamic speaker mix"));
    addAction(new SetClipVoiceContextAction(false, singerInfo, speakerInfo,
                                            normalizeSpeakerMixData(data), clip));
}

void SpeakerMixActions::applyClipSpeakerMixPreset(const SingerInfo &singerInfo,
                                                  const SpeakerInfo &speakerInfo,
                                                  const SpeakerMixData &data, SingingClip *clip) {
    setTranslatableName("SpeakerMixActions",
                        QT_TRANSLATE_NOOP("SpeakerMixActions", "Apply speaker mix preset"));
    addAction(new SetClipVoiceContextAction(false, singerInfo, speakerInfo,
                                            normalizeSpeakerMixData(data), clip));
}

void SpeakerMixActions::selectTrackSingleSpeaker(const SingerInfo &singerInfo,
                                                 const SpeakerInfo &speakerInfo, Track *track) {
    setTranslatableName("SpeakerMixActions",
                        QT_TRANSLATE_NOOP("SpeakerMixActions", "Select speaker"));
    addAction(new SetTrackVoiceContextAction(singerInfo, speakerInfo, {}, track));
}

void SpeakerMixActions::applyTrackSpeakerMixPreset(const SingerInfo &singerInfo,
                                                   const SpeakerInfo &speakerInfo,
                                                   const SpeakerMixData &data, Track *track) {
    setTranslatableName("SpeakerMixActions",
                        QT_TRANSLATE_NOOP("SpeakerMixActions", "Apply speaker mix preset"));
    addAction(new SetTrackVoiceContextAction(singerInfo, speakerInfo, normalizeSpeakerMixData(data),
                                             track));
}

void SpeakerMixActions::replaceTrackSpeakerMix(const SpeakerMixData &data, Track *track) {
    setTranslatableName("SpeakerMixActions",
                        QT_TRANSLATE_NOOP("SpeakerMixActions", "Edit track speaker mix"));
    const auto oldContext = track ? track->voiceContext() : EffectiveVoiceContext();
    const auto newData = preservePresetSourceAsDirty(oldContext.speakerMix, data);
    addAction(
        new SetTrackVoiceContextAction(oldContext.singer, oldContext.speaker, newData, track));
}
