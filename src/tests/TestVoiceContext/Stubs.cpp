#include <lite/ProjectModel/AppModel/SingingClip.h>
#include "AppContext.h"
#include <lite/Core/IdGenerator.h>

template <>
IdGenerator *AppContext::instance<IdGenerator>() {
    return nullptr;
}

void SingingClip::setTrackVoiceContext(const SingerInfo &, const SpeakerInfo &,
                                       const SpeakerMixModel::SpeakerMixData &) {
}
