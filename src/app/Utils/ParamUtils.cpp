//
// Created by fluty on 24-10-21.
//

#include "ParamUtils.h"

const QStringList &ParamUtils::names() const {
    return m_names;
}

QString ParamUtils::nameFromType(ParamInfo::Name name) const {
    return m_names[name];
}

const ParamProperties *ParamUtils::getPropertiesByName(ParamInfo::Name name) const {
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
        case ParamInfo::Gender:
            return &genderProperties;
        case ParamInfo::Velocity:
            return &velocityProperties;
        case ParamInfo::Unknown:
            return &defaultProperties;
    }
    return &defaultProperties;
}