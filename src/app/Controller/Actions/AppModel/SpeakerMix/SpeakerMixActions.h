//
// Created by FlutyDeer on 2026/6/20.
//

#ifndef SPEAKERMIXACTIONS_H
#define SPEAKERMIXACTIONS_H

#include "Model/AppModel/SpeakerMixData.h"
#include "Modules/History/ActionSequence.h"

class SingingClip;
using SpeakerMixModel::SpeakerMixData;

class SpeakerMixActions : public ActionSequence {
public:
    void replaceSpeakerMix(const SpeakerMixData &data, SingingClip *clip);
};

#endif // SPEAKERMIXACTIONS_H
