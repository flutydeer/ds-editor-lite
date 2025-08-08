//
// Created by fluty on 24-2-23.
//

#include "Params.h"

#include "SingingClip.h"

#include <QDebug>

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
    qFatal() << "Param type out of range" << name;
    return nullptr;
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
