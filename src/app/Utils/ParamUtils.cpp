//
// Created by fluty on 24-10-21.
//

#include "ParamUtils.h"

ParamUtils::ParamUtils(QObject *parent) : QObject(parent) {
}

ParamUtils::~ParamUtils() = default;

LITE_SINGLETON_IMPLEMENT_INSTANCE(ParamUtils)

const QStringList &ParamUtils::names() const {
    return m_names;
}

QString ParamUtils::nameFromType(const ParamInfo::Name name) const {
    return m_names[name];
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
        case ParamInfo::Unknown:
            return &defaultProperties;
    }
    return &defaultProperties;
}