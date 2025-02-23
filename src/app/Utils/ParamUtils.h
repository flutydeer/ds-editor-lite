//
// Created by fluty on 24-9-19.
//

#ifndef PARAMUTILS_H
#define PARAMUTILS_H

#define paramUtils ParamUtils::instance()

#include "Model/AppModel/Params.h"
#include "Utils/Singleton.h"
#include "Model/AppModel/ParamProperties.h"

#include <QObject>

class ParamUtils final : public QObject, public Singleton<ParamUtils> {
    Q_OBJECT
public:
    [[nodiscard]] const QStringList &names() const;
    [[nodiscard]] QString nameFromType(ParamInfo::Name name) const;
    [[nodiscard]] const ParamProperties *getPropertiesByName(ParamInfo::Name name) const;

private:
    // Names and keys
    const QStringList m_names = {tr("Pitch"),       tr("Expressiveness"), tr("Energy"),
                                 tr("Breathiness"), tr("Voicing"),        tr("Tension"),
                                 tr("Gender"),      tr("Velocity"),       tr("Tone Shift")};
    const QStringList m_keys = {"pitch",   "expressiveness", "energy",   "breathiness", "voicing",
                                "tension", "gender",         "velocity", "tone_shift"};

    // Properties
    const ParamProperties defaultProperties;
    const PitchParamProperties pitchProperties;
    const ExprParamProperties exprProperties;
    const DecibelParamProperties decibelProperties;
    const TensionParamProperties tensionProperties;
    const GenderParamProperties genderProperties;
    const VelocityParamProperties velocityProperties;
    const ToneShiftParamProperties toneShiftProperties;
};


#endif // PARAMUTILS_H
