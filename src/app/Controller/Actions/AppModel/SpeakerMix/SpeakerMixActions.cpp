//
// Created by FlutyDeer on 2026/6/20.
//

#include "SpeakerMixActions.h"

#include "ApplyClipSpeakerMixPresetAction.h"
#include "ApplyTrackSpeakerMixPresetAction.h"
#include "ReplaceSpeakerMixAction.h"
#include "ReplaceTrackSpeakerMixAction.h"

void SpeakerMixActions::replaceSpeakerMix(const SpeakerMixData &data, SingingClip *clip) {
    setName(tr("Edit speaker mix"));
    addAction(new ReplaceSpeakerMixAction(data, clip));
}

void SpeakerMixActions::applyClipSpeakerMixPreset(const SingerInfo &singerInfo,
                                                  const SpeakerInfo &speakerInfo,
                                                  const SpeakerMixData &data, SingingClip *clip) {
    setName(tr("Apply speaker mix preset"));
    addAction(new ApplyClipSpeakerMixPresetAction(singerInfo, speakerInfo, data, clip));
}

void SpeakerMixActions::applyTrackSpeakerMixPreset(const SingerInfo &singerInfo,
                                                   const SpeakerInfo &speakerInfo,
                                                   const SpeakerMixData &data, Track *track) {
    setName(tr("Apply speaker mix preset"));
    addAction(new ApplyTrackSpeakerMixPresetAction(singerInfo, speakerInfo, data, track));
}

void SpeakerMixActions::replaceTrackSpeakerMix(const SpeakerMixData &data, Track *track) {
    setName(tr("Edit track speaker mix"));
    addAction(new ReplaceTrackSpeakerMixAction(data, track));
}
