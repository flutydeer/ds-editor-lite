#include "Model/AppModel/SingingClip.h"
#include "AppContext.h"
#include "Utils/IdGenerator.h"

template <>
IdGenerator *AppContext::instance<IdGenerator>() {
    return nullptr;
}

void SingingClip::setTrackVoiceContext(const SingerInfo &, const SpeakerInfo &,
                                       const SpeakerMixModel::SpeakerMixData &) {
}
