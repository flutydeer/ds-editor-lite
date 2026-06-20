//
// Created by FlutyDeer on 2026/6/20.
//

#include "SpeakerMixActions.h"

#include "ReplaceSpeakerMixAction.h"

void SpeakerMixActions::replaceSpeakerMix(const SpeakerMixData &data, SingingClip *clip) {
    setName(tr("Edit speaker mix"));
    addAction(new ReplaceSpeakerMixAction(data, clip));
}
