#include <lite/ProjectModel/AppModel/Params.h>

#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/ParamProperties.h>

#include <QDebug>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logParams, "model.params")

Param::~Param() {
    auto dispose = [=](const QList<Curve *> &curves) {
        for (int i = 0; i < curves.count(); i++) {
            delete curves[i];
        }
    };
    dispose(m_edited);
    dispose(m_envelope);
    dispose(m_original);
    dispose(m_unknown);
}

const QList<Curve *> &Param::curves(const Type type) const {
    switch (type) {
        case Original:
            return m_original;
        case Edited:
            return m_edited;
        case Envelope:
            return m_envelope;
        case Unknown:
        default:
            return m_unknown;
    }
}

void Param::setCurves(const Type type, const QList<Curve *> &curves, SingingClip *clip) {
    for (const auto &curve : curves)
        curve->setClip(clip);
    switch (type) {
        case Original:
            m_original = curves;
            break;
        case Edited:
            m_edited = curves;
            break;
        case Envelope:
            m_envelope = curves;
            break;
        case Unknown:
            break;
    }
}

ParamInfo::ParamInfo(SingingClip *clip) {
    m_clip = clip;
}

Param *ParamInfo::getParamByName(const Name name) {
    switch (name) {
        case Pitch:
            return &pitch;
        case Breathiness:
            return &breathiness;
        case Tension:
            return &tension;
        case Velocity:
            return &velocity;
        case Voicing:
            return &voicing;
        case Expressiveness:
            return &expressiveness;
        case Gender:
            return &gender;
        case Energy:
            return &energy;
        case MouthOpening:
            return &mouthOpening;
        case ToneShift:
            return &toneShift;
        default:
            break;
    }
    qCCritical(logParams) << "Param type out of range" << name;
    return nullptr;
}

const Param *ParamInfo::getParamByName(const Name name) const {
    return const_cast<ParamInfo *>(this)->getParamByName(name);
}

bool ParamInfo::hasOriginalParam(const Name name) {
    switch (name) {
        case ParamInfo::Pitch:
        case ParamInfo::Breathiness:
        case ParamInfo::Voicing:
        case ParamInfo::Energy:
        case ParamInfo::Tension:
        case ParamInfo::MouthOpening:
            return true;
        default:
            return false;
    }
}

ParamInfo::ValueSpec ParamInfo::valueSpec(const Name name) {
    const ParamProperties *properties = nullptr;
    const ParamProperties defaults;
    const PitchParamProperties pitch;
    const ExprParamProperties expressiveness;
    const DecibelParamProperties decibels;
    const TensionParamProperties tension;
    const MouthOpeningParamProperties mouthOpening;
    const GenderParamProperties gender;
    const VelocityParamProperties velocity;
    const ToneShiftParamProperties toneShift;
    switch (name) {
        case Pitch:
            properties = &pitch;
            break;
        case Expressiveness:
            properties = &expressiveness;
            break;
        case Energy:
        case Breathiness:
        case Voicing:
            properties = &decibels;
            break;
        case Tension:
            properties = &tension;
            break;
        case MouthOpening:
            properties = &mouthOpening;
            break;
        case Gender:
            properties = &gender;
            break;
        case Velocity:
            properties = &velocity;
            break;
        case ToneShift:
            properties = &toneShift;
            break;
        case SpeakerMix:
        case Unknown:
            properties = &defaults;
            break;
    }
    return {.minimum = properties->minimum,
            .maximum = properties->maximum,
            .step = 1,
            .unit = properties->unit};
}
