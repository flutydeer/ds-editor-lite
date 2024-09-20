//
// Created by fluty on 24-2-23.
//

#include "Params.h"

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

const QList<Curve *> &Param::curves(Type type) const {
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

void Param::setCurves(Type type, const QList<Curve *> &curves) {
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

Param *ParamInfo::getParamByName(Name name) {
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
        case Unknown: {
            return nullptr;
        }
    }
    qFatal() << "Param type out of range" << name;
    return nullptr;
}