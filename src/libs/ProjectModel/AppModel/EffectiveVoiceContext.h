#ifndef EFFECTIVEVOICECONTEXT_H
#define EFFECTIVEVOICECONTEXT_H

#include <lite/ProjectModel/AppModel/SpeakerMixData.h>
#include <lite/ProjectModel/Voice/SingerInfo.h>
#include <lite/ProjectModel/Voice/SpeakerInfo.h>

struct EffectiveVoiceContext {
    SingerInfo singer;
    SpeakerInfo speaker;
    SpeakerMixModel::SpeakerMixData speakerMix;
    bool followsTrack = false;

    bool operator==(const EffectiveVoiceContext &other) const {
        return singer == other.singer && speaker == other.speaker &&
               speakerMix == other.speakerMix && followsTrack == other.followsTrack;
    }

    bool operator!=(const EffectiveVoiceContext &other) const {
        return !(*this == other);
    }

    bool hasSameInferenceInput(const EffectiveVoiceContext &other) const {
        return singer == other.singer && speaker == other.speaker && speakerMix == other.speakerMix;
    }
};

struct VoiceContextChange {
    EffectiveVoiceContext before;
    EffectiveVoiceContext after;
};

#endif // EFFECTIVEVOICECONTEXT_H
