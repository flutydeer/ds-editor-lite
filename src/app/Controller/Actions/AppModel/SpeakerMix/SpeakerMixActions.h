//
// Created by FlutyDeer on 2026/6/20.
//

#ifndef SPEAKERMIXACTIONS_H
#define SPEAKERMIXACTIONS_H

#include "Model/AppModel/SpeakerMixData.h"
#include "Modules/History/ActionSequence.h"

class SingingClip;
class Track;
class SingerInfo;
class SpeakerInfo;
using SpeakerMixModel::SpeakerMixData;

class SpeakerMixActions : public ActionSequence {
public:
    void replaceSpeakerMix(const SpeakerMixData &data, SingingClip *clip);
    void enableClipDynamicSpeakerMix(const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo,
                                     const SpeakerMixData &data, SingingClip *clip);
    void applyClipSpeakerMixPreset(const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo,
                                   const SpeakerMixData &data, SingingClip *clip);
    void selectTrackSingleSpeaker(const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo,
                                  Track *track);
    void applyTrackSpeakerMixPreset(const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo,
                                    const SpeakerMixData &data, Track *track);
    void replaceTrackSpeakerMix(const SpeakerMixData &data, Track *track);
};

#endif // SPEAKERMIXACTIONS_H
