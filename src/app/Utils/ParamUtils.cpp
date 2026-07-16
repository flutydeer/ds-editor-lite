//
// Created by fluty on 24-10-21.
//

#include "ParamUtils.h"

ParamUtils::ParamUtils(QObject *parent) : QObject(parent) {
}

ParamUtils::~ParamUtils() = default;

LITE_SINGLETON_IMPLEMENT_INSTANCE(ParamUtils)

QStringList ParamUtils::names() const {
    return {tr("Pitch"),    tr("Expressiveness"), tr("Energy"),        tr("Breathiness"),
            tr("Voicing"),  tr("Tension"),        tr("Mouth Opening"), tr("Gender"),
            tr("Velocity"), tr("Tone Shift"),     tr("Speaker Mix")};
}

QString ParamUtils::nameFromType(const ParamInfo::Name name) const {
    const auto index = static_cast<int>(name);
    const auto translatedNames = names();
    if (index < 0 || index >= translatedNames.size())
        return {};
    return translatedNames[index];
}

const ParamProperties *ParamUtils::getPropertiesByName(const ParamInfo::Name name) const {
    switch (name) {
        case ParamInfo::Pitch:
            return &pitchProperties;
        case ParamInfo::Expressiveness:
            return &exprProperties;
        case ParamInfo::Energy:
        case ParamInfo::Breathiness:
        case ParamInfo::Voicing:
            return &decibelProperties;
        case ParamInfo::Tension:
            return &tensionProperties;
        case ParamInfo::MouthOpening:
            return &mouthOpeningProperties;
        case ParamInfo::Gender:
            return &genderProperties;
        case ParamInfo::Velocity:
            return &velocityProperties;
        case ParamInfo::ToneShift:
            return &toneShiftProperties;
        case ParamInfo::SpeakerMix:
        case ParamInfo::Unknown:
            return &defaultProperties;
    }
    return &defaultProperties;
}
