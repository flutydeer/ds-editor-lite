//
// Created by fluty on 24-2-23.
//

#include "Params.h"
const OverlappableSerialList<Curve> &Param::curves(ParamType type)const {
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
void Param::setCurves(ParamType type, const OverlappableSerialList<Curve> &curves) {
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
void Param::setCurves(ParamType type, const QList<Curve *> &curves) {
    OverlappableSerialList<Curve> list;
    for (const auto curve : curves)
        list.add(curve);
    setCurves(type, list);
}
Param *ParamBundle::getParamByName(ParamName name) {
    switch (name) {
        case Pitch:
            return &pitch;
        case Energy:
            return &energy;
        case Tension:
            return &tension;
        case Breathiness:
            return &breathiness;
        case Unknown:
        default:
            return nullptr;
    }
}