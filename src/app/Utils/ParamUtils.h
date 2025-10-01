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

class ParamUtils final : public QObject {
    Q_OBJECT

    explicit ParamUtils(QObject *parent = nullptr);
    ~ParamUtils() override;

public:
    LITE_SINGLETON_DECLARE_INSTANCE(ParamUtils)
    Q_DISABLE_COPY_MOVE(ParamUtils)

    const QStringList &names() const;
    QString nameFromType(ParamInfo::Name name) const;
    const ParamProperties *getPropertiesByName(ParamInfo::Name name) const;

private:
    // Names and keys
    const QStringList m_names = {
        tr("Pitch"),   tr("Expressiveness"), tr("Energy"), tr("Breathiness"), tr("Voicing"),
        tr("Tension"), tr("Mouth Opening"),  tr("Gender"), tr("Velocity"),    tr("Tone Shift")};
    const QStringList m_keys = {"pitch",   "expressiveness", "energy", "breathiness", "voicing",
                                "tension", "mouth_opening",  "gender", "velocity",    "tone_shift"};

    // Properties
    const ParamProperties defaultProperties;
    const PitchParamProperties pitchProperties;
    const ExprParamProperties exprProperties;
    const DecibelParamProperties decibelProperties;
    const TensionParamProperties tensionProperties;
    const MouthOpeningParamProperties mouthOpeningProperties;
    const GenderParamProperties genderProperties;
    const VelocityParamProperties velocityProperties;
    const ToneShiftParamProperties toneShiftProperties;
};


#endif // PARAMUTILS_H
